#!/bin/bash
# 스케치를 PC 에서 테스트 : tests/run.sh
# 아두이노 IDE 처럼 함수 선언을 첫 함수 앞에 끼워 넣은 뒤, 가짜 아두이노(stub/)와 함께 g++ 로 컴파일해서 실행한다.
set -e
cd "$(dirname "$0")"
B=$(mktemp -d)
python3 - ../team2_bt_pid/team2_bt_pid.ino "$B/team2_bt_pid.ino" <<'EOF'
import re, sys
src = open(sys.argv[1]).read().split('\n')
pat = re.compile(r'^(void|bool|int|long|char)\s+(\w+)\s*\(([^)]*)\)\s*\{')
protos, first = [], None
for i, l in enumerate(src):
    m = pat.match(l)
    if m:
        first = i if first is None else first
        protos.append(f'{m.group(1)} {m.group(2)}({m.group(3)});')
src[first:first] = protos
open(sys.argv[2], 'w').write('\n'.join(src))
EOF
c++ -std=c++17 -w -Istub -I"$B" test_pid.cpp -o "$B/test_pid"
"$B/test_pid"
