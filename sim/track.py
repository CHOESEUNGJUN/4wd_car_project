"""트랙 맵 주행 시뮬레이션 : 트랙 위에서 버전 C 로직(라인센서 3개, PID, 코너, 선 놓침)으로 달린다

  python sim/track.py team2                       2팀 트랙 : 출발 -> 바깥 고리 합류 -> 2바퀴 (판 이음새 요철 ±0.2mm)
  python sim/track.py team2 --corner-in -110 --battery 6.2 --sensor-gap 1.5
  python sim/track.py oval                        예제 타원
  python sim/track.py sim/tracks/my.png --start 20,50,0 --lap 20,50 --laps 2
  python sim/track.py team2 --replay gulno_KP50_KD30_xxx.csv   실제 주행 기록(굴노 디버그 앱 CSV)과 비교

내장 트랙(team2, oval)은 sim/tracks/<이름>.png 로 그림도 저장한다.
직접 그린 PNG : 흰(밝은) 바탕에 검은 선. --scale 은 픽셀 하나가 몇 cm (기본 0.5)
  --start x,y,방향 : 차 가운데 cm (왼쪽 아래가 0,0), 방향 도 (0 = 오른쪽, 90 = 위)
  --lap x,y : 이 점을 지날 때마다 셈. 처음 지날 때(합류) 다음부터가 1바퀴
결과 : sim/results/<날짜>_track_<이름>.png (트랙 + 주행 경로, 색 = 단계) / .csv (20ms 마다 시간·위치·방향·단계·오차)
"""
import argparse
import csv
import datetime
import math
import os
import random
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from model import Params, simulate, _shape, _Ground, _loads  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
TAPE_CM = 1.9          # 절연테이프 폭


# ---------- 내장 트랙 (칸 단위, 그림처럼 y 는 아래로) ----------
def _arc(cx, cy, r, a0, a1, n=40):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
             cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def _rounded_rect(w, h, r):
    return (_arc(r, r, r, 180, 270) + _arc(w - r, r, r, 270, 360)
            + _arc(w - r, h - r, r, 0, 90) + _arc(r, h - r, r, 90, 180) + [(0, r)])


def team2():
    """실제 2팀 코스 (2026-11-09 사진, 폰 S21 울트라 16.5cm 로 크기 맞춤). 단위 cm, 그림처럼 y 는 아래로.
    출발 = 가운데 짧은 선 끝에서 위로 -> 오른쪽 U 턴 -> 안쪽 고리(오른쪽 아래 -> 왼쪽) -> 왼쪽 위에서 S 곡선
    -> 불룩한 반원을 돌아 위 변에 합류(왼쪽으로) -> 바깥 고리 2바퀴. 퍼즐 판 한 변 약 17.3cm"""
    W = 71.5                                       # 바깥 고리 한 변 (테이프 가운데 기준)
    outer = _rounded_rect(W, W, 8)
    entry = ([(34.2, 45.7)]                        # 출발선 끝
             + _arc(43.1, 26.2, 8.9, 180, 360)      # 위로 올라가 오른쪽 U 턴
             + _arc(45.0, 47.1, 7.0, 0, 90)         # 오른쪽 아래 모서리
             + _arc(23.8, 47.1, 7.0, 90, 180)       # 왼쪽 아래 모서리
             + _arc(23.8, 25.0, 7.0, 180, 270)      # 왼쪽 위 모서리 -> 오른쪽으로
             + _arc(25.5, 9.0, 9.0, 90, -90))       # 불룩한 반원 -> 위 변에 왼쪽으로 합류
    return {'paths': [outer, entry], 'w': W, 'h': W, 'cell': 1, 'margin': 5, 'scale': 0.2,
            'start': (34.2, 45.7, 90), 'start_is_sensor': True, 'lap': (0, W / 2),
            'panel_cells': 17.3, 'panel_origin': (-9.1, -7.6)}


def oval():
    return {'paths': [_rounded_rect(200 / 30, 110 / 30, 45 / 30)], 'w': 200 / 30, 'h': 110 / 30,
            'start': (65 / 30, 110 / 30, 0), 'lap': (65 / 30, 110 / 30), 'panel_cells': 0}


BUILTIN = {'team2': team2, 'oval': oval}


def render(defn, cell, scale, line_cm=TAPE_CM, margin_cells=0.5):
    """칸 단위 선 -> 흑백 그림 (0 = 검정). 돌려줌 : 그림, 칸(x, y아래) -> 세계 cm 변환 함수"""
    W = int((defn['w'] + 2 * margin_cells) * cell / scale)
    H = int((defn['h'] + 2 * margin_cells) * cell / scale)
    img = np.ones((H, W))
    Hc = defn['h'] + 2 * margin_cells

    def to_cm(x, y):                                  # 세계 좌표 cm (왼쪽 아래 0,0, y 위로)
        return (x + margin_cells) * cell, (Hc - y - margin_cells) * cell

    half = line_cm / 2 / scale                         # 픽셀
    for path in defn['paths']:
        pts = [(cx / scale, (H * scale - cy) / scale) for cx, cy in (to_cm(*p) for p in path)]   # 열, 행
        for (c0, r0), (c1, r1) in zip(pts, pts[1:]):
            lo_c, hi_c = int(max(0, min(c0, c1) - half - 1)), int(min(W, max(c0, c1) + half + 2))
            lo_r, hi_r = int(max(0, min(r0, r1) - half - 1)), int(min(H, max(r0, r1) + half + 2))
            cc, rr = np.meshgrid(np.arange(lo_c, hi_c) + .5, np.arange(lo_r, hi_r) + .5)
            dc, dr = c1 - c0, r1 - r0
            t = np.clip(((cc - c0) * dc + (rr - r0) * dr) / max(dc * dc + dr * dr, 1e-9), 0, 1)
            d = np.hypot(cc - c0 - t * dc, rr - r0 - t * dr)
            img[lo_r:hi_r, lo_c:hi_c][d <= half] = 0
    return img, to_cm


class Track:
    def __init__(self, img, scale):
        if img.ndim == 3:
            img = img[..., :3].mean(axis=2)
        self.img, self.dark = img, img < 0.5          # 검정 = 선
        self.H, self.W = img.shape
        self.scale = scale                            # cm / 픽셀

    def black(self, x_m, y_m):
        col = int(x_m * 100 / self.scale)
        row = self.H - 1 - int(y_m * 100 / self.scale)
        return 1 if 0 <= row < self.H and 0 <= col < self.W and self.dark[row, col] else 0


class PanelGround:
    """판(정사각형)마다 높이 -amp ~ +amp. 이음새에서 계단"""
    def __init__(self, size_m, amp_m, x0, y0, rnd):
        self.s, self.a, self.x0, self.y0, self.r, self.f = size_m, amp_m, x0, y0, rnd, {}

    def h(self, x, y):
        k = (math.floor((x - self.x0) / self.s), math.floor((self.y0 - y) / self.s))
        if k not in self.f:
            self.f[k] = self.r.uniform(-self.a, self.a)
        return self.f[k]


def min_start_pwm(p):
    """킥 없이 바닥에서 출발하는 가장 작은 PWM (무게 x 중력, 감속기 정지 마찰, 배터리 포함)"""
    q = Params(**{**p.__dict__, 'shaping': False, 'bump_cm': 0})
    for pwm in range(60, 256, 5):
        if simulate(q, pwm, pwm, T=0.5, v_start=0.0)['fwd_cm'] > 1.0:
            return pwm
    return None


def run(track, ground, p, a):
    """버전 C 의 pidLine() 을 그대로 흉내 내며 물리 모델로 달린다"""
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
    integ = dterm = 0.0                               # 버전 C 상태
    last_err = last_side = 0
    lost_since = cross_since = None
    cmdL = cmdR = 0
    stage, err = 'LINE', 0
    log, lost_events, prev_stage = [], 0, ''
    dt, t, dist = 4e-4, 0.0, 0.0
    lx, ly = a.lap[0] / 100, a.lap[1] / 100
    passes, dist_at_pass, start_s = [], -1.0, None
    while t < a.time:
        if t >= next_pid:                             # 20ms 마다 pidLine()
            next_pid = t + 0.02
            L, C, R = (track.black(X + x * math.cos(TH) - y * math.sin(TH), Y + x * math.sin(TH) + y * math.cos(TH))
                       for (x, y) in sensors)
            stop = False
            lost_pid = False
            if L and C and R:                         # 3개 모두 검정 : 0.5초 계속이면 선 끝
                lost_since = None
                cross_since = t if cross_since is None else cross_since
                stage, stop = ('END', True) if t - cross_since >= 0.5 else ('CROSS', False)
                e = 0
            elif not (L or C or R):                   # 선 놓침 : 1.5초면 정지, 그동안 마지막 쪽으로 크게
                cross_since = None
                lost_since = t if lost_since is None else lost_since
                lost = t - lost_since
                limit = (a.lost_pid_ms + a.lost_ms + (a.back_ms if last_side else 0)) / 1000   # PID -> 마지막 쪽 -> 반대쪽
                stage, stop = ('LOST', True) if lost >= limit else ('NOLINE', False)
                lost_pid = lost < a.lost_pid_ms / 1000
                e = (last_side if lost < (a.lost_pid_ms + a.lost_ms) / 1000 else -last_side) * 3
            else:
                cross_since = lost_since = None
                stage = 'LINE'
                e = {(0, 1, 0): 0, (0, 1, 1): 1, (0, 0, 1): 2, (1, 1, 0): -1, (1, 0, 0): -2}.get((L, C, R), 0)
                if e:
                    last_side = 1 if e > 0 else -1
            if stop:
                cmdL = cmdR = 0
                integ = dterm = 0
                last_err = 0
            elif abs(e) >= 2 and not lost_pid:        # 코너·선 놓침 : 바깥 CORNER_OUT, 안쪽 CORNER_IN
                cmdL, cmdR = (a.corner_out, a.corner_in) if e > 0 else (a.corner_in, a.corner_out)
                if abs(e) == 2:
                    stage = 'CORNER'
                integ = dterm = 0
                last_err = e
            else:                                     # PID
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
            log.append((round(t, 2), round(X * 100, 1), round(Y * 100, 1), round(math.degrees(TH) % 360), stage, e,
                        _shape(cmdL, dict(stL), t, p), _shape(cmdR, dict(stR), t, p)))
            if stop:
                break
            if start_s is None and t > 2.0:
                stage = 'NOSTART'
                break
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
        if start_s is None and dist > 0.05:
            start_s = t
        if math.hypot(X - lx, Y - ly) < 0.08 and dist - dist_at_pass > 1.0:
            passes.append(round(t, 1))                # 첫 번째 = 합류(또는 출발), 그다음부터 바퀴
            dist_at_pass = dist
            if len(passes) - 1 >= a.laps:
                stage = 'DONE'
                break
        t += dt
    laps = [round(b - a_, 1) for a_, b in zip(passes, passes[1:])]
    return log, {'result': stage, 'start_s': start_s and round(start_s, 2), 'merge_s': passes[0] if passes else None,
                 'laps_s': laps, 'lost_events': lost_events, 'dist_m': round(dist, 2), 't_end': round(t, 1)}


def load_real(path):
    """굴노 디버그 앱 CSV -> 자동 주행 줄만, 0초 = 자동이 되고 HOLD(0.5초 정지)가 끝난 순간"""
    with open(path, encoding='utf-8') as f:
        rows = [r for r in csv.DictReader(f) if r['mode'] == 'a']
    go = next((r for r in rows if r['stage'] != 'HOLD'), None)
    if go is None:
        sys.exit('자동 주행 기록이 없습니다')
    t0 = int(go['t_ms'])
    out = []
    for r in rows:
        t = (int(r['t_ms']) - t0) / 1000
        if t < 0:
            continue
        num = lambda k: int(r[k]) if r.get(k, '').lstrip('-').isdigit() else None
        out.append({'t': t, 'stage': r['stage'], 'err': num('err'), })
    return out


def save_compare(track, sim_log, real, info, a, name):
    """위 : 맵 (시뮬 경로, 5초마다 시각) / 아래 : 같은 시간축에 실제·시뮬 오차, 단계.
    실제 위치는 기록에 없다. 0.1초 모터 값으로 경로를 되살려 보면 몇 초 만에 어긋나서(0.02초마다 바뀌는 조향을 놓침) 쓰지 않는다.
    실제 그래프에서 이상한 시각을 찾고 -> 맵에서 시뮬이 그 시각에 어디였는지 보고 -> 실제 차가 거기서 뭘 했는지 비교"""
    out = os.path.join(HERE, 'results')
    os.makedirs(out, exist_ok=True)
    path = os.path.join(out, datetime.datetime.now().strftime('%Y-%m-%d_%H%M') + f'_replay_{name}')
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    Wc, Hc = track.W * track.scale, track.H * track.scale
    fig = plt.figure(figsize=(11, 10 * Hc / Wc + 6))
    gs = fig.add_gridspec(3, 1, height_ratios=[10 * Hc / Wc, 2.2, 1])
    ax = fig.add_subplot(gs[0])
    ax.imshow(track.img, cmap='gray', extent=[0, Wc, 0, Hc], vmin=0, vmax=1)
    ax.plot([r[1] for r in sim_log], [r[2] for r in sim_log], c='tab:green', lw=1.5, label='sim (PID)')
    for r in sim_log:                                 # 5초마다 시각
        if abs(r[0] % 5) < 0.01 and r[0] > 0:
            ax.plot(r[1], r[2], 'k.', ms=5)
            ax.annotate(f'{r[0]:.0f}s', (r[1], r[2]), fontsize=8, xytext=(3, 3), textcoords='offset points')
    ax.plot(a.start[0], a.start[1], 'b*', ms=14)
    ax.set_title(f"{name} | KP {a.kp} KD {a.kd} CORNER {a.corner_out}/{a.corner_in} bat {a.battery}V mass {a.mass}kg | sim: {info['result']} merge {info['merge_s']}s laps {info['laps_s']}", fontsize=9)
    ax.legend(loc='upper left', fontsize=8)
    ax2 = fig.add_subplot(gs[1])
    ax2.step([r['t'] for r in real if r['err'] is not None], [r['err'] for r in real if r['err'] is not None], where='post', c='tab:red', label='real err')
    ax2.step([r[0] for r in sim_log], [r[5] for r in sim_log], where='post', c='tab:green', alpha=.6, label='sim err')
    ax2.set_ylabel('err'); ax2.set_yticks(range(-3, 4)); ax2.grid(alpha=.3); ax2.legend(fontsize=8, loc='upper right')
    ax3 = fig.add_subplot(gs[2], sharex=ax2)
    sc = {'LINE': 'tab:green', 'CORNER': 'tab:orange', 'CROSS': 'tab:blue', 'NOLINE': 'tab:red', 'LOST': 'black', 'END': 'black', 'OBST': 'tab:gray'}
    for y, rows in ((1, [(r['t'], r['stage']) for r in real]), (0, [(r[0], r[4]) for r in sim_log])):
        for (t0, st), (t1, _) in zip(rows, rows[1:]):
            ax3.barh(y, t1 - t0, left=t0, color=sc.get(st, 'white'), height=.8)
    ax3.set_yticks([0, 1]); ax3.set_yticklabels(['sim', 'real']); ax3.set_xlabel('s (0 = auto start)')
    fig.tight_layout()
    fig.savefig(path + '.png', dpi=110)
    return path


def save(track, log, info, a, name, panel_cm, origin):
    out = os.path.join(HERE, 'results')
    os.makedirs(out, exist_ok=True)
    path = os.path.join(out, datetime.datetime.now().strftime('%Y-%m-%d_%H%M') + f'_track_{name}')
    with open(path + '.csv', 'w', newline='', encoding='utf-8') as f:
        wr = csv.writer(f)
        wr.writerow(['t_s', 'x_cm', 'y_cm', 'heading_deg', 'stage', 'err', 'motorL', 'motorR'])
        wr.writerows(log)
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    Wc, Hc = track.W * track.scale, track.H * track.scale
    plt.figure(figsize=(10, 10 * Hc / Wc + 1))
    plt.imshow(track.img, cmap='gray', extent=[0, Wc, 0, Hc], vmin=0, vmax=1)
    if panel_cm:                                      # 판 이음새 (요철)
        x = origin[0] + panel_cm
        while x < Wc:
            plt.axvline(x, color='tab:red', ls=':', lw=1)
            x += panel_cm
        y = origin[1] - panel_cm
        while y > 0:
            plt.axhline(y, color='tab:red', ls=':', lw=1)
            y -= panel_cm
    colors = {'LINE': 'tab:green', 'CORNER': 'tab:orange', 'CROSS': 'tab:blue', 'NOLINE': 'tab:red', 'LOST': 'black', 'END': 'black'}
    for st, c in colors.items():
        pts = [(r[1], r[2]) for r in log if r[4] == st]
        if pts:
            plt.scatter([q[0] for q in pts], [q[1] for q in pts], s=3, c=c, label=st)
    plt.plot(a.start[0], a.start[1], 'b*', ms=14, label='start')
    plt.plot(a.lap[0], a.lap[1], 'mo', ms=9, mfc='none', label='lap point')
    plt.title(f"KP {a.kp} KI {a.ki} KD {a.kd} BASE {a.base} CORNER {a.corner_out}/{a.corner_in} "
              f"bat {a.battery}V mu {a.mu}\n{info['result']} | merge {info['merge_s']}s laps {info['laps_s']} "
              f"| lost {info['lost_events']}x | {info['dist_m']} m", fontsize=10)
    plt.xlabel('cm')
    plt.legend(loc='upper left', fontsize=7)
    plt.tight_layout()
    plt.savefig(path + '.png', dpi=110)
    return path


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('track', help='team2 / oval / PNG 경로')
    ap.add_argument('--cell', type=float, default=30, help='내장 트랙 : 회색 칸 한 변 cm')
    ap.add_argument('--scale', type=float, default=0.5, help='cm / 픽셀')
    ap.add_argument('--start', help='x,y,방향 (cm, cm, 도)')
    ap.add_argument('--lap', help='x,y (cm) 바퀴 세는 점')
    ap.add_argument('--laps', type=int, default=2)
    ap.add_argument('--time', type=float, default=400, help='최대 시간 초')
    ap.add_argument('--panel', type=float, help='판 한 변 cm (이음새 요철). 내장 트랙은 자동')
    ap.add_argument('--panel-mm', type=float, default=0.2, help='판 높이 ±mm')
    ap.add_argument('--bump', type=float, default=0.0, help='판 대신 3cm 격자 무작위 요철 cm')
    ap.add_argument('--kp', type=float, default=50)
    ap.add_argument('--ki', type=float, default=0.2)
    ap.add_argument('--kd', type=float, default=30)
    ap.add_argument('--base', type=int, default=120)
    ap.add_argument('--corner-out', type=int, default=180)
    ap.add_argument('--corner-in', type=int, default=-90)
    ap.add_argument('--sensor-x', type=float, default=12.0, help='차 가운데에서 가운데 라인센서까지 앞쪽 거리 cm (사진 짐작)')
    ap.add_argument('--sensor-gap', type=float, default=4.9, help='라인센서 사이 간격 cm (실측)')
    ap.add_argument('--lost-pid-ms', type=int, default=500, help='선 놓침 : 먼저 PID 로 보정하는 ms (LOST_PID_MS)')
    ap.add_argument('--lost-ms', type=int, default=500, help='그다음 마지막 쪽으로 크게 꺾으며 찾는 ms (LOST_STOP_MS)')
    ap.add_argument('--back-ms', type=int, default=1000, help='그래도 없으면 반대쪽으로 찾는 ms (BACK_SEARCH_MS, 0 = 바로 정지)')
    ap.add_argument('--mass', type=float, default=0.25, help='차 무게 kg')
    ap.add_argument('--mu', type=float, default=0.5)
    ap.add_argument('--battery', type=float, default=7.0)
    ap.add_argument('--seed', type=int, default=0)
    ap.add_argument('--replay', help='굴노 디버그 앱 CSV : 실제 주행과 시뮬을 같은 시각으로 비교')
    a = ap.parse_args()

    if a.track in BUILTIN:
        d = BUILTIN[a.track]()
        cell = d.get('cell', a.cell)
        a.scale = d.get('scale', a.scale)             # 작은 트랙은 더 촘촘한 그림
        img, to_cm = render(d, cell, a.scale, margin_cells=d.get('margin', 0.5))
        os.makedirs(os.path.join(HERE, 'tracks'), exist_ok=True)
        import matplotlib.image as mpimg
        mpimg.imsave(os.path.join(HERE, 'tracks', a.track + '.png'), img, cmap='gray', vmin=0, vmax=1)
        sx, sy = to_cm(*d['start'][:2])
        if d.get('start_is_sensor'):                  # 출발점 = 센서 위치 -> 차 가운데는 sensor_x 만큼 뒤
            h = math.radians(d['start'][2])
            sx, sy = sx - a.sensor_x * math.cos(h), sy - a.sensor_x * math.sin(h)
        a.start = a.start or f"{sx},{sy},{d['start'][2]}"
        a.lap = a.lap or '%s,%s' % to_cm(*d['lap'])
        panel_cm = a.panel if a.panel is not None else d['panel_cells'] * cell
        origin = to_cm(*d.get('panel_origin', (0, 0)))                          # 판 이음새는 바깥 고리 왼쪽 위에서부터
        name = a.track
    else:
        import matplotlib.image as mpimg
        img = mpimg.imread(a.track)
        if not a.start:
            ap.error('PNG 트랙은 --start x,y,방향 이 필요')
        a.lap = a.lap or ','.join(a.start.split(',')[:2])
        panel_cm = a.panel or 0
        origin = (0, img.shape[0] * a.scale)
        name = os.path.splitext(os.path.basename(a.track))[0]
    a.start = [float(v) for v in a.start.split(',')]
    a.lap = [float(v) for v in a.lap.split(',')]

    p = Params(mu=a.mu, battery_v=a.battery, bump_cm=a.bump, mass=a.mass)
    rnd = random.Random(a.seed)
    ground = (PanelGround(panel_cm / 100, a.panel_mm / 1000, origin[0] / 100, origin[1] / 100, rnd) if panel_cm
              else _Ground(a.bump / 100, p.bump_cell, rnd))
    print(f"무게 {a.mass}kg x {p.g} = {a.mass * p.g:.2f}N, 배터리 {a.battery}V : 킥 없이 출발하는 최소 PWM = {min_start_pwm(p)} "
          f"(BASE {a.base}, 출발 순간 킥 {p.kick_pwm} {p.kick_ms}ms)")
    tr = Track(img, a.scale)
    if a.replay:
        real = load_real(a.replay)
        a.time = real[-1]['t']                        # 시뮬도 실제와 같은 길이만큼
        log, info = run(tr, ground, p, a)
        path = save_compare(tr, log, real, info, a, os.path.splitext(os.path.basename(a.replay))[0])
        print('시뮬 :', info)
        print(f"실제 : {real[-1]['t']:.1f}초 기록, 선 놓침 {sum(1 for x, y in zip(real, real[1:]) if y['stage'] == 'NOLINE' and x['stage'] != 'NOLINE')}번")
        print(f"저장 : {path}.png")
        sys.exit()
    log, info = run(tr, ground, p, a)
    path = save(tr, log, info, a, name, panel_cm, origin)
    print(info)
    print(f"저장 : {path}.png / .csv")
