"""시뮬레이션 실행 (우분투 서버에서)

  python sim/run.py corner            experiments.yaml 의 corner 실험
  python sim/run.py start             출발 실험
  python sim/run.py --list            실험 목록
  python sim/run.py --calibrate       measured: 실측값에 맞는 모르는 값(L298N 손실, 감속기 마찰, 마찰계수) 찾기

결과 : sim/results/<날짜_시각>_<실험>.csv + .png  (조건별 평균)
조건 목록의 모든 조합 x seeds 번을 CPU 코어 수만큼 나눠 돌린다.
"""
import argparse
import csv
import datetime
import itertools
import os
import sys
from multiprocessing import Pool, cpu_count

import yaml

sys.path.insert(0, os.path.dirname(__file__))
from model import Params, simulate  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'results')


def load():
    with open(os.path.join(HERE, 'experiments.yaml'), encoding='utf-8') as f:
        return yaml.safe_load(f)


def as_list(v):
    return v if isinstance(v, list) else [v]


def one(job):
    params, kind, cmd, seed, T = job
    p = Params(**params)
    if kind == 'corner':        # 오른쪽 코너 : 왼쪽 바깥, 오른쪽 안쪽
        return simulate(p, cmd['out'], cmd['in'], T=T, v_start=0.25, seed=seed)
    return simulate(p, cmd['pwm'], cmd['pwm'], T=T, v_start=0.0, seed=seed)   # 출발 : 정지에서 양쪽 같은 PWM


def grid(d):
    keys = list(d)
    for vals in itertools.product(*(as_list(d[k]) for k in keys)):
        yield dict(zip(keys, vals))


def run_experiment(name, exp, base):
    kind = exp['kind']
    seeds = exp.get('seeds', 5)
    T = exp.get('T', 0.8)
    keys, jobs = [], []
    for pv in grid(exp.get('params', {})):
        for cv in grid(exp['commands']):
            params = {**base, **pv}
            n = seeds if params.get('bump_cm', 0.2) > 0 else 1
            keys.append((pv, cv, n))
            jobs += [(params, kind, cv, s, T) for s in range(n)]
    print(f"[{name}] 조합 {len(keys)}개, 계산 {len(jobs)}번, 코어 {cpu_count()}개")
    with Pool() as pool:
        results = pool.map(one, jobs, chunksize=1)
    rows, i = [], 0
    for pv, cv, n in keys:
        rs = results[i:i + n]
        i += n
        rows.append({**pv, **cv,
                     'yaw_deg': round(sum(r['yaw_deg'] for r in rs) / n, 2),
                     'yaw_min': round(min(r['yaw_deg'] for r in rs), 2),
                     'fwd_cm': round(sum(r['fwd_cm'] for r in rs) / n, 2),
                     'fwd_min': round(min(r['fwd_cm'] for r in rs), 2),
                     'inner_stall': round(sum(r['inner_stall'] for r in rs) / n, 3),
                     'lift': round(sum(r['lift'] for r in rs) / n, 3)})
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, datetime.datetime.now().strftime('%Y-%m-%d_%H%M') + f'_{name}')
    with open(path + '.csv', 'w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    plot(rows, kind, path + '.png')
    for r in rows:
        print(r)
    print(f"저장: {path}.csv / .png")


def plot(rows, kind, path):
    """x 축 = 명령(코너 in / 출발 pwm), 선 하나 = 나머지 조건 조합"""
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        return
    xk, yk = ('in', 'yaw_deg') if kind == 'corner' else ('pwm', 'fwd_cm')
    skip = {xk, 'out', 'yaw_deg', 'yaw_min', 'fwd_cm', 'fwd_min', 'inner_stall', 'lift'}
    groups = {}
    for r in rows:
        label = ', '.join(f'{k}={r[k]}' for k in r if k not in skip)
        groups.setdefault(label, []).append((r[xk], r[yk]))
    plt.figure(figsize=(9, 5))
    for label, pts in groups.items():
        pts.sort()
        plt.plot([p[0] for p in pts], [p[1] for p in pts], marker='o', label=label or 'default')
    plt.xlabel('CORNER_IN' if kind == 'corner' else 'PWM')
    plt.ylabel('turned deg' if kind == 'corner' else 'distance cm')
    plt.grid(alpha=.3)
    plt.legend(fontsize=7)
    plt.tight_layout()
    plt.savefig(path, dpi=120)


def _score(args):
    params, meas = args
    err = 0.0
    for m in meas.get('start') or []:        # {battery_v, pwm, moves}
        q = Params(**{**params, 'battery_v': m['battery_v'], 'shaping': False})
        moved = simulate(q, m['pwm'], m['pwm'], T=0.5, v_start=0.0, seed=0)['fwd_cm'] > 1.0
        err += 0 if moved == m['moves'] else 10
    for m in meas.get('corner') or []:       # {battery_v, out, in, yaw_deg}
        q = Params(**{**params, 'battery_v': m['battery_v']})
        err += abs(simulate(q, m['out'], m['in'], T=0.8, seed=0)['yaw_deg'] - m['yaw_deg']) / 10
    return err, params


def calibrate(cfg):
    meas, space, base = cfg['measured'], cfg['calibrate'], cfg.get('base', {})
    cands = [{**base, **g} for g in grid(space)]
    print(f"후보 {len(cands)}개")
    with Pool() as pool:
        scored = pool.map(_score, [(c, meas) for c in cands], chunksize=1)
    scored.sort(key=lambda s: s[0])
    for err, p in scored[:8]:
        print(f"오차 {err:6.2f} : " + ', '.join(f'{k}={p[k]}' for k in space))
    print("가장 잘 맞는 값을 experiments.yaml 의 base: 에 적으면 이후 실험에 쓰인다")


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('name', nargs='?')
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--calibrate', action='store_true')
    a = ap.parse_args()
    cfg = load()
    if a.calibrate:
        calibrate(cfg)
    elif a.list or not a.name:
        print('실험:', ', '.join(cfg['experiments']))
    else:
        run_experiment(a.name, cfg['experiments'][a.name], cfg.get('base', {}))
