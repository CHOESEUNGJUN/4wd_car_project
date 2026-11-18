% SPDX-License-Identifier: MIT · Copyright (c) 2026 4wd-car Team 2 "Gulno" (굴러라 노란바퀴) · see LICENSE, NOTICE.md
% pwm_duty_simulink.m : 우리 차 Timer1 (삼각파, 분주 256, 122.5Hz) 에서 analogWrite 값마다 PWM 파형을 본다. 설명 : docs/PWM.md 4장
% 값 하나 = 스코프 하나 : 노랑 = 카운트(삼각파), 파랑 = PWM 출력 (켜짐 255 / 꺼짐 0). 가로 선 = analogWrite 값

vals = [90 100 120];                         % 보고 싶은 analogWrite 값 (0~255). 바꿔서 다시 Run
mdl = 'pwm_duty';
if bdIsLoaded(mdl), close_system(mdl, 0); end
new_system(mdl); open_system(mdl);
set_param(mdl, 'StopTime', '0.02', 'MaxStep', '1e-6');   % 20ms

clk = 16e6; TOP = 255; pre = 256;
T = 2*TOP*pre/clk;                                         % 한 주기 8.16ms (122.5Hz)

for k = 1:numel(vals)
  v = vals(k); n = sprintf('PWM_%d', v);
  y = 50 + (k-1)*130; b = [mdl '/' n];
  add_block('simulink/Sources/Repeating Sequence', [b '_cnt'], 'rep_seq_t', sprintf('[0 %g %g]', T/2, T), 'rep_seq_y', sprintf('[0 %d 0]', TOP), 'Position', [30 y 90 y+30]);
  add_block('simulink/Sources/Constant', [b '_analogWrite'], 'Value', num2str(v), 'Position', [30 y+60 90 y+90]);
  add_block('simulink/Logic and Bit Operations/Relational Operator', [b '_cmp'], 'Operator', '<', 'Position', [150 y+25 180 y+65]);
  add_block('simulink/Signal Attributes/Data Type Conversion', [b '_dbl'], 'OutDataTypeStr', 'double', 'Position', [210 y+30 250 y+60]);
  add_block('simulink/Math Operations/Gain', [b '_x255'], 'Gain', '255', 'Position', [280 y+30 310 y+60]);
  add_block('simulink/Signal Routing/Mux', [b '_mux'], 'Inputs', '3', 'Position', [350 y 355 y+90]);
  add_block('simulink/Sinks/Scope', [b '_scope'], 'Position', [390 y+30 420 y+60]);
  L = @(a, c) add_line(mdl, [n a], [n c], 'autorouting', 'on');
  L('_cnt/1', '_cmp/1');  L('_analogWrite/1', '_cmp/2');  L('_cmp/1', '_dbl/1');
  L('_dbl/1', '_x255/1'); L('_cnt/1', '_mux/1');  L('_x255/1', '_mux/2');
  L('_analogWrite/1', '_mux/3');                            % 가로 선도 같이 보기
  L('_mux/1', '_scope/1');
  fprintf('analogWrite %3d : 듀티 %2.0f%%, 한 번 켜짐 %.2f ms / 주기 %.2f ms\n', v, v/TOP*100, v/TOP*T*1e3, T*1e3);
end

sim(mdl);
for k = 1:numel(vals), open_system(sprintf('%s/PWM_%d_scope', mdl, vals(k))); end
