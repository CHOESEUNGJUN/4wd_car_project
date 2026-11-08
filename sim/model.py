"""굴노 4륜 미니카 물리 모델 (코너, 출발).

차를 위에서 본 평면 강체로 놓고, 바퀴 4개 각각의 모터·감속기·바닥 마찰을 0.4ms 간격으로 계산한다.
- 모터 : TT 기어모터 1:48, 바퀴 축 기준 선형 토크-속도 (멈춤 토크, 무부하 속도)
- 전원 : 니켈수소 팩 전압 - 배터리 내부저항 x 전류 - L298N 손실(drop0 + drop_k x 채널 전류)
- 감속기 : 정지 마찰(stiction) 을 넘어야 돌기 시작, 돌 때는 gear_friction
- 바닥 : 바퀴 접지점 미끄럼 속도 방향으로 마찰 mu x 하중 (4륜은 옆으로 미끄러져야 방향이 바뀜)
- 요철 : 3cm 격자마다 0 ~ bump_cm 높이, 단단한 차체 + 타이어 스프링으로 바퀴별 하중 계산 (바퀴가 뜰 수 있음)
- 코드와 같은 힘 다듬기 : 0 이 아니면 최소 min_pwm, 방향이 바뀌는 순간 kick_ms 동안 kick_pwm
숫자는 전부 Params 로 바꾼다. 실제로 잰 값이 아닌 것은 calibrate.py 로 맞춘다.
"""
import math
import random
from dataclasses import dataclass, asdict


@dataclass
class Params:
    # 차
    mass: float = 0.65          # kg
    g: float = 9.8
    wheel_r: float = 0.0325     # m (지름 65mm)
    track: float = 0.13         # 좌우 바퀴 간격 m
    wheelbase: float = 0.12     # 앞뒤 바퀴 간격 m
    body_w: float = 0.15        # 요 관성 계산용 차체 크기 m
    body_l: float = 0.20
    # 바닥
    mu: float = 0.5             # 마찰계수 (나무합판 + 단단한 고무 0.4~0.5)
    v_slip0: float = 0.01       # 마찰 정규화 속도 m/s
    k_tire: float = 5000.0      # 타이어 수직 강성 N/m
    rolling: float = 0.03       # 구름 저항 (하중 비율)
    bump_cm: float = 0.2        # 요철 최대 높이 cm
    bump_cell: float = 0.03     # 요철 간격 m
    # 모터 (TT 1:48, 바퀴 축 기준)
    motor_v: float = 6.0
    motor_rpm: float = 200.0    # motor_v 에서 무부하
    stall_torque: float = 0.078 # motor_v 에서 멈춤 토크 N·m
    stall_current: float = 1.5  # motor_v 에서 멈춤 전류 A
    gear_friction: float = 0.008  # 돌 때 감속기 마찰 N·m
    stiction: float = 0.018     # 멈춰 있을 때 감속기 정지 마찰 N·m
    wheel_j: float = 3e-4       # 바퀴 + 감속비^2 x 회전자 관성 kg·m^2
    # 전원
    battery_v: float = 7.0      # 니켈수소 5개 충전 직후 7.0, 많이 쓴 상태 5.8
    battery_r: float = 0.5      # 팩 내부저항 Ω
    drop0: float = 1.0          # L298N 손실 = drop0 + drop_k x 채널 전류
    drop_k: float = 0.5
    # 코드와 같은 힘 다듬기
    min_pwm: int = 90
    kick_pwm: int = 255
    kick_ms: int = 120
    shaping: bool = True


def _shape(cmd, st, t, p):
    if not p.shaping:
        return cmd
    s = (cmd > 0) - (cmd < 0)
    if s == 0:
        st['sign'] = 0
        return 0
    if s != st['sign']:
        st['sign'] = s
        st['kick'] = t + p.kick_ms / 1000
    a = min(abs(cmd), 255)
    if t < st['kick']:
        a = p.kick_pwm
    elif a < p.min_pwm:
        a = p.min_pwm
    return s * a


class _Ground:
    """3cm 격자마다 0 ~ bump 높이, 양선형 보간"""
    def __init__(self, bump_m, cell, rnd):
        self.b, self.c, self.r, self.f = bump_m, cell, rnd, {}

    def h(self, x, y):
        if self.b <= 0:
            return 0.0
        i, j = math.floor(x / self.c), math.floor(y / self.c)
        fx, fy = x / self.c - i, y / self.c - j

        def at(a, b):
            k = (a, b)
            if k not in self.f:
                self.f[k] = self.r.uniform(0, self.b)
            return self.f[k]
        return (at(i, j) * (1 - fx) * (1 - fy) + at(i + 1, j) * fx * (1 - fy)
                + at(i, j + 1) * (1 - fx) * fy + at(i + 1, j + 1) * fx * fy)


def _solve3(a, b):
    a = [row[:] for row in a]
    b = b[:]
    for c in range(3):
        piv = max(range(c, 3), key=lambda r: abs(a[r][c]))
        a[c], a[piv] = a[piv], a[c]
        b[c], b[piv] = b[piv], b[c]
        for r in range(c + 1, 3):
            f = a[r][c] / a[c][c]
            for k in range(c, 3):
                a[r][k] -= f * a[c][k]
            b[r] -= f * b[c]
    x = [0.0] * 3
    for r in (2, 1, 0):
        x[r] = (b[r] - sum(a[r][k] * x[k] for k in range(r + 1, 3))) / a[r][r]
    return x


def _loads(hs, wheels, p):
    """단단한 차체(평면 z = z0 + px + qy) + 타이어 스프링. 힘·모멘트 평형, 뜬 바퀴는 빼고 다시"""
    act = [0, 1, 2, 3]
    mg = p.mass * p.g
    for _ in range(4):
        if len(act) < 3:
            n = [0.0] * 4
            for i in act:
                n[i] = mg / len(act)
            return n
        A = [[0.0] * 3 for _ in range(3)]
        b = [0.0] * 3
        for i in act:
            x, y, _ = wheels[i]
            row = (1, x, y)
            for r in range(3):
                for c in range(3):
                    A[r][c] += p.k_tire * row[r] * row[c]
                b[r] += p.k_tire * row[r] * hs[i]
        b[0] -= mg
        z0, px, qy = _solve3(A, b)
        n = [0.0] * 4
        neg = []
        for i in act:
            x, y, _ = wheels[i]
            n[i] = p.k_tire * (hs[i] - z0 - px * x - qy * y)
            if n[i] < 0:
                neg.append(i)
        if not neg:
            return n
        act = [i for i in act if i not in neg]
    return n


def simulate(p: Params, left_cmd, right_cmd, T=0.8, v_start=0.25, seed=0, dt=4e-4):
    """왼쪽·오른쪽 PWM(-255~255) 을 T 초 동안 준다. v_start 는 시작 속도 (직진 중 코너 0.25, 출발 0).
    돌려주는 값 : yaw_deg(오른쪽으로 돈 각도), fwd_cm(앞으로 간 거리), inner_stall(오른쪽 바퀴가 멈춘 비율), lift(바퀴가 뜬 비율)"""
    rnd = random.Random(seed)
    ground = _Ground(p.bump_cm / 100, p.bump_cell, rnd)
    wheels = [(+p.wheelbase / 2, +p.track / 2, 'L'), (+p.wheelbase / 2, -p.track / 2, 'R'),
              (-p.wheelbase / 2, +p.track / 2, 'L'), (-p.wheelbase / 2, -p.track / 2, 'R')]
    iz = p.mass * (p.body_l ** 2 + p.body_w ** 2) / 12
    w_nl = p.motor_rpm * 2 * math.pi / 60
    r_m = p.motor_v / p.stall_current
    ke = p.motor_v / w_nl
    t_per_v = p.stall_torque / p.motor_v
    t_per_w = p.stall_torque / w_nl
    X = Y = TH = 0.0
    vx, vy, w = v_start, 0.0, 0.0
    ww = [v_start / p.wheel_r] * 4
    s0 = 1 if v_start > 0 else 0
    stL, stR = {'sign': s0, 'kick': 0}, {'sign': s0, 'kick': 0}
    n_load = [p.mass * p.g / 4] * 4
    next_load = 0.0
    lifted = samples = stall = 0
    steps = 0
    t = 0.0
    while t < T:
        pl = _shape(left_cmd, stL, t, p)
        pr = _shape(right_cmd, stR, t, p)
        if t >= next_load:
            hs = []
            for (x, y, _) in wheels:
                gx = X + x * math.cos(TH) - y * math.sin(TH)
                gy = Y + x * math.sin(TH) + y * math.cos(TH)
                hs.append(ground.h(gx, gy))
            n_load = _loads(hs, wheels, p)
            next_load = t + 0.005
            lifted += sum(1 for n in n_load if n < 0.05 * p.mass * p.g / 4)
            samples += 4
        # 전압 : 팩 - 내부저항 x 전류 - L298N 손실
        v_nom = p.battery_v - p.drop0
        i_tot = 0.0
        for k, (_, _, side) in enumerate(wheels):
            pw = pl if side == 'L' else pr
            i_tot += abs(v_nom * pw / 255 - ke * ww[k]) / r_m
        i_tot = min(i_tot, 6.0)
        vm = max(0.0, v_nom - i_tot * p.battery_r - p.drop_k * i_tot / 2)
        fx_b = fy_b = mz = 0.0
        for k, (x, y, side) in enumerate(wheels):
            pw = pl if side == 'L' else pr
            tau = t_per_v * vm * pw / 255 - t_per_w * ww[k]
            vix, viy = vx - w * y, vy + w * x
            sx, sy = vix - ww[k] * p.wheel_r, viy
            s = math.hypot(sx, sy)
            f = p.mu * n_load[k] * min(1.0, s / p.v_slip0)
            fx = -f * sx / s if s > 1e-9 else 0.0
            fy = -f * sy / s if s > 1e-9 else 0.0
            net = tau - fx * p.wheel_r
            resist = p.rolling * n_load[k] * p.wheel_r
            if abs(ww[k]) < 0.05 and abs(net) < p.stiction + resist:
                ww[k] = 0.0
            else:
                sgn = (ww[k] > 0) - (ww[k] < 0) or ((net > 0) - (net < 0))
                ww[k] += (net - (p.gear_friction + resist) * sgn) / p.wheel_j * dt
            fx_b += fx
            fy_b += fy
            mz += x * fy - y * fx
            if side == 'R' and abs(ww[k]) < 0.5:
                stall += 1
        vx += (fx_b / p.mass + w * vy) * dt
        vy += (fy_b / p.mass - w * vx) * dt
        w += mz / iz * dt
        X += (vx * math.cos(TH) - vy * math.sin(TH)) * dt
        Y += (vx * math.sin(TH) + vy * math.cos(TH)) * dt
        TH += w * dt
        t += dt
        steps += 1
    return {'yaw_deg': -math.degrees(TH), 'fwd_cm': X * 100,
            'inner_stall': stall / (steps * 2), 'lift': lifted / max(samples, 1)}


def params_dict(p):
    return asdict(p)
