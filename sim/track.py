"""트랙 맵 주행 시뮬레이션 : 그린 트랙 위에서 버전 C 로직으로 한 바퀴

  python sim/track.py --make-oval sim/tracks/oval.png      예제 트랙(타원) 만들기
  python sim/track.py sim/tracks/oval.png                    달리기 (기본 30초)
  python sim/track.py sim/tracks/my.png --start 20,50,0 --kp 50 --kd 30 --corner-in -90 --time 40

트랙 PNG : 흰(밝은) 바탕에 검은 선. 그림판 등으로 그리면 된다. --scale 은 픽셀 하나가 몇 cm (기본 0.5)
시작 위치 --start x,y,방향 : cm 단위(왼쪽 아래가 0,0), 방향은 도(0 = 오른쪽, 90 = 위). 차 가운데를 선 위에 둔다
결과 : sim/results/<날짜>_track_<이름>.png (트랙 + 주행 경로, 색 = 단계) / .csv (시간·위치·단계·오차)
"""
import argparse
import csv
import datetime
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(__file__))
from model import Params, _shape, _Ground, _loads  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))


def make_oval(path, w_cm=200, h_cm=110, line_cm=2.0, scale=0.5):
    """가로 w x 세로 h cm 안에 양 끝이 둥근 타원 고리 (검은 선)"""
    import numpy as np
    import matplotlib.image as mpimg
    W, H = int(w_cm / scale), int(h_cm / scale)
    img = np.ones((H, W))
    r = (h_cm - 20) / 2                     # 둥근 끝 반지름 cm
    cy, x0, x1 = h_cm / 2, 10 + r, w_cm - 10 - r
    for row in range(H):
        for col in range(W):
            x, y = (col + .5) * scale, (H - row - .5) * scale
            if x0 <= x <= x1:
                d = abs(abs(y - cy) - r)
            else:
                cx = x0 if x < x0 else x1
                d = abs(math.hypot(x - cx, y - cy) - r)
            if d <= line_cm / 2:
                img[row, col] = 0
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    mpimg.imsave(path, img, cmap='gray', vmin=0, vmax=1)
    print(f"만듦 : {path}  ({w_cm}x{h_cm}cm, 선 {line_cm}cm). 시작 예 : --start {x0 + 10:.0f},{cy - r:.0f},0")


class Track:
    def __init__(self, path, scale):
        import matplotlib.image as mpimg
        img = mpimg.imread(path)
        if img.ndim == 3:
            img = img[..., :3].mean(axis=2)
        self.dark = img < 0.5                # 검정 = 선
        self.H, self.W = self.dark.shape
        self.scale = scale                   # cm / 픽셀
        self.img = img

    def black(self, x_m, y_m):
        col = int(x_m * 100 / self.scale)
        row = self.H - 1 - int(y_m * 100 / self.scale)
        if 0 <= row < self.H and 0 <= col < self.W:
            return 1 if self.dark[row, col] else 0
        return 0


def run(track, p, a):
    """버전 C 의 pidLine() 을 그대로 흉내 내며 물리 모델로 달린다"""
    rnd = random.Random(a.seed)
    ground = _Ground(p.bump_cm / 100, p.bump_cell, rnd)
    wheels = [(+p.wheelbase / 2, +p.track / 2, 'L'), (+p.wheelbase / 2, -p.track / 2, 'R'),
              (-p.wheelbase / 2, +p.track / 2, 'L'), (-p.wheelbase / 2, -p.track / 2, 'R')]
    sensors = [(a.sensor_x / 100, +a.sensor_gap / 100), (a.sensor_x / 100, 0.0), (a.sensor_x / 100, -a.sensor_gap / 100)]
    iz = p.mass * (p.body_l ** 2 + p.body_w ** 2) / 12
    w_nl = p.motor_rpm * 2 * math.pi / 60
    r_m, ke = p.motor_v / p.stall_current, p.motor_v / w_nl
    t_per_v, t_per_w = p.stall_torque / p.motor_v, p.stall_torque / w_nl
    sx0, sy0, sh = a.start
    X, Y, TH = sx0 / 100, sy0 / 100, math.radians(sh)
    vx = vy = w = 0.0
    ww = [0.0] * 4
    stL, stR = {'sign': 0, 'kick': 0}, {'sign': 0, 'kick': 0}
    n_load = [p.mass * p.g / 4] * 4
    next_load = next_pid = 0.0
    # 버전 C 상태
    integ = dterm = 0.0
    last_err = last_side = 0
    lost_since = cross_since = None
    cmdL = cmdR = 0
    stage = 'LINE'
    err = 0
    log, lost_events, prev_stage = [], 0, ''
    dt, t, dist = 4e-4, 0.0, 0.0
    lap_t = None
    while t < a.time:
        if t >= next_pid:                    # 20ms 마다 pidLine()
            next_pid = t + 0.02
            ss = []
            for (x, y) in sensors:
                gx = X + x * math.cos(TH) - y * math.sin(TH)
                gy = Y + x * math.sin(TH) + y * math.cos(TH)
                ss.append(track.black(gx, gy))
            L, C, R = ss
            stop = False
            if L and C and R:
                cross_since = cross_since if cross_since is not None else t
                if t - cross_since >= 0.5:
                    stage, stop = 'END', True
                e = 0
            else:
                cross_since = None
            if not stop and not L and not C and not R:
                lost_since = lost_since if lost_since is not None else t
                if t - lost_since >= 1.5:
                    stage, stop = 'LOST', True
                else:
                    stage = 'NOLINE'
                e = last_side * 3
            elif not stop and not (L and C and R):
                lost_since = None
                stage = 'LINE'
                e = {(0, 1, 0): 0, (0, 1, 1): 1, (0, 0, 1): 2, (1, 1, 0): -1, (1, 0, 0): -2}.get((L, C, R), 0)
                if e:
                    last_side = 1 if e > 0 else -1
            if stop:
                cmdL = cmdR = 0
                integ = dterm = 0
                last_err = 0
            elif abs(e) >= 2:                # 코너 : 바깥 CORNER_OUT, 안쪽 CORNER_IN
                if e > 0:
                    cmdL, cmdR = a.corner_out, a.corner_in
                else:
                    cmdL, cmdR = a.corner_in, a.corner_out
                if abs(e) == 2:
                    stage = 'CORNER'
                integ = dterm = 0
                last_err = e
            else:                            # PID
                if (e > 0 > integ) or (e < 0 < integ):
                    integ = 0
                integ = max(-50, min(50, integ + e * 0.02))
                dterm = dterm * 0.8 + a.kd * (e - last_err)
                last_err = e
                u = a.kp * e + a.ki * integ + dterm
                cmdL = max(0, min(255, a.base + int(u)))
                cmdR = max(0, min(255, a.base - int(u)))
            err = e
            if stage in ('NOLINE', 'LOST') and prev_stage not in ('NOLINE', 'LOST'):
                lost_events += 1
            prev_stage = stage
            log.append((round(t, 2), round(X * 100, 1), round(Y * 100, 1), round(math.degrees(TH) % 360), stage, e))
        # 물리 (model.simulate 와 같은 계산)
        pl, pr = _shape(cmdL, stL, t, p), _shape(cmdR, stR, t, p)
        if t >= next_load:
            hs = [ground.h(X + x * math.cos(TH) - y * math.sin(TH), Y + x * math.sin(TH) + y * math.cos(TH)) for (x, y, _) in wheels]
            n_load = _loads(hs, wheels, p)
            next_load = t + 0.005
        v_nom = p.battery_v - p.drop0
        i_tot = min(6.0, sum(abs(v_nom * (pl if s == 'L' else pr) / 255 - ke * ww[k]) / r_m for k, (_, _, s) in enumerate(wheels)))
        vm = max(0.0, v_nom - i_tot * p.battery_r - p.drop_k * i_tot / 2)
        fxb = fyb = mz = 0.0
        for k, (x, y, s) in enumerate(wheels):
            pw = pl if s == 'L' else pr
            tau = t_per_v * vm * pw / 255 - t_per_w * ww[k]
            sxv, syv = vx - w * y - ww[k] * p.wheel_r, vy + w * x
            sp = math.hypot(sxv, syv)
            f = p.mu * n_load[k] * min(1.0, sp / p.v_slip0)
            fx = -f * sxv / sp if sp > 1e-9 else 0.0
            fy = -f * syv / sp if sp > 1e-9 else 0.0
            net = tau - fx * p.wheel_r
            res = p.rolling * n_load[k] * p.wheel_r
            if abs(ww[k]) < 0.05 and abs(net) < p.stiction + res:
                ww[k] = 0.0
            else:
                sg = (ww[k] > 0) - (ww[k] < 0) or ((net > 0) - (net < 0))
                ww[k] += (net - (p.gear_friction + res) * sg) / p.wheel_j * dt
            fxb += fx
            fyb += fy
            mz += x * fy - y * fx
        vx += (fxb / p.mass + w * vy) * dt
        vy += (fyb / p.mass - w * vx) * dt
        w += mz / iz * dt
        dX = (vx * math.cos(TH) - vy * math.sin(TH)) * dt
        dY = (vx * math.sin(TH) + vy * math.cos(TH)) * dt
        X, Y, TH = X + dX, Y + dY, TH + w * dt
        dist += math.hypot(dX, dY)
        if lap_t is None and dist > 1.0 and math.hypot(X - sx0 / 100, Y - sy0 / 100) < 0.08:
            lap_t = t                        # 출발점 근처로 돌아옴 = 한 바퀴
        t += dt
    return log, {'lap_s': lap_t, 'dist_m': dist, 'lost_events': lost_events, 'end_stage': stage}


def save(track, log, info, a, name):
    out = os.path.join(HERE, 'results')
    os.makedirs(out, exist_ok=True)
    path = os.path.join(out, datetime.datetime.now().strftime('%Y-%m-%d_%H%M') + f'_track_{name}')
    with open(path + '.csv', 'w', newline='', encoding='utf-8') as f:
        wr = csv.writer(f)
        wr.writerow(['t_s', 'x_cm', 'y_cm', 'heading_deg', 'stage', 'err'])
        wr.writerows(log)
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    ext = [0, track.W * track.scale, 0, track.H * track.scale]
    plt.figure(figsize=(10, 10 * track.H / track.W + 1))
    plt.imshow(track.img, cmap='gray', extent=ext, vmin=0, vmax=1)
    colors = {'LINE': 'tab:green', 'CORNER': 'tab:orange', 'NOLINE': 'tab:red', 'LOST': 'black', 'END': 'black'}
    for st, c in colors.items():
        pts = [(r[1], r[2]) for r in log if r[4] == st]
        if pts:
            plt.scatter([p[0] for p in pts], [p[1] for p in pts], s=4, c=c, label=st)
    plt.plot(log[0][1], log[0][2], 'b*', ms=14, label='start')
    lap = f"lap {info['lap_s']:.1f}s" if info['lap_s'] else 'no lap'
    plt.title(f"KP {a.kp} KI {a.ki} KD {a.kd} BASE {a.base} CORNER {a.corner_out}/{a.corner_in} | {lap}, "
              f"lost {info['lost_events']}x, {info['dist_m']:.1f} m, end {info['end_stage']}")
    plt.xlabel('cm')
    plt.legend(loc='upper right', fontsize=8)
    plt.tight_layout()
    plt.savefig(path + '.png', dpi=110)
    return path


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('track', nargs='?')
    ap.add_argument('--make-oval', metavar='PNG')
    ap.add_argument('--scale', type=float, default=0.5, help='cm / 픽셀')
    ap.add_argument('--start', default='65,10,0', help='x,y,방향 (cm, cm, 도)')
    ap.add_argument('--time', type=float, default=30)
    ap.add_argument('--kp', type=float, default=50)
    ap.add_argument('--ki', type=float, default=0.2)
    ap.add_argument('--kd', type=float, default=30)
    ap.add_argument('--base', type=int, default=120)
    ap.add_argument('--corner-out', type=int, default=220)
    ap.add_argument('--corner-in', type=int, default=-90)
    ap.add_argument('--sensor-x', type=float, default=7.0, help='차 가운데에서 라인센서까지 앞쪽 거리 cm')
    ap.add_argument('--sensor-gap', type=float, default=1.5, help='라인센서 사이 간격 cm')
    ap.add_argument('--mu', type=float, default=0.5)
    ap.add_argument('--battery', type=float, default=7.0)
    ap.add_argument('--bump', type=float, default=0.2, help='요철 cm')
    ap.add_argument('--seed', type=int, default=0)
    a = ap.parse_args()
    if a.make_oval:
        make_oval(a.make_oval, scale=a.scale)
        sys.exit()
    if not a.track:
        ap.error('트랙 PNG 를 주거나 --make-oval 로 만드세요')
    a.start = [float(v) for v in a.start.split(',')]
    p = Params(mu=a.mu, battery_v=a.battery, bump_cm=a.bump)
    tr = Track(a.track, a.scale)
    log, info = run(tr, p, a)
    path = save(tr, log, info, a, os.path.splitext(os.path.basename(a.track))[0])
    print(info)
    print(f"저장 : {path}.png / .csv")
