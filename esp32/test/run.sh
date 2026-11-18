#!/bin/sh
# SPDX-License-Identifier: MIT · Copyright (c) 2026 4wd-car Team 2 "Gulno" (굴러라 노란바퀴) · see LICENSE, NOTICE.md
# 신호 판단 시험 : 보드 없이 PC 에서. 코드를 고친 뒤 올리기 전에 돌림
cd "$(dirname "$0")" && python3 gen.py && c++ -std=c++17 -O1 -w -o t t.cpp && ./t
