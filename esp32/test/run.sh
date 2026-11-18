#!/bin/sh
# 신호 판단 시험 : 보드 없이 PC 에서. 코드를 고친 뒤 올리기 전에 돌림
cd "$(dirname "$0")" && python3 gen.py && c++ -std=c++17 -O1 -w -o t t.cpp && ./t
