% SPDX-License-Identifier: MIT · Copyright (c) 2026 4wd-car Team 2 "Gulno" (굴러라 노란바퀴) · see LICENSE, NOTICE.md
% pwm_timers_simulink.m : Simulink 모델 pwm_timers 를 만들어 돌리고 스코프 3개를 띄운다. 설명 : docs/PWM.md
% 한 줄 = 타이머 하나 : [카운터 (Repeating Sequence)] -> [ < OCR ] -> PWM 출력. 스코프에 카운트(노랑)와 출력(파랑)

mdl = 'pwm_timers';
if bdIsLoaded(mdl), close_system(mdl, 0); end
new_system(mdl); open_system(mdl);
set_param(mdl, 'StopTime', '0.02', 'MaxStep', '1e-6');   % 20ms

clk = 16e6; TOP = 255; OCR = round(0.30 * TOP);           % 30% 선 = 77
cfg = {'Timer0_saw_64',  'saw', 64
       'Timer0_saw_256', 'saw', 256
       'Timer1_tri_256', 'tri', 256};

for k = 1:3
  [n, shape, pre] = cfg{k, :};
  y = 50 + (k-1)*130; b = [mdl '/' n];
  if strcmp(shape, 'saw')                                 % 톱니파 : 0 -> 255, 뚝
    T = (TOP+1)*pre/clk; ts = sprintf('[0 %g]', T);      ys = sprintf('[0 %d]', TOP);
  else                                                    % 삼각파 : 0 -> 255 -> 0
    T = 2*TOP*pre/clk;   ts = sprintf('[0 %g %g]', T/2, T); ys = sprintf('[0 %d 0]', TOP);
  end
  add_block('simulink/Sources/Repeating Sequence', [b '_cnt'], 'rep_seq_t', ts, 'rep_seq_y', ys, 'Position', [30 y 90 y+30]);
  add_block('simulink/Sources/Constant', [b '_OCR'], 'Value', num2str(OCR), 'Position', [30 y+60 90 y+90]);
  add_block('simulink/Logic and Bit Operations/Relational Operator', [b '_cmp'], 'Operator', '<', 'Position', [150 y+25 180 y+65]);
  add_block('simulink/Signal Attributes/Data Type Conversion', [b '_dbl'], 'OutDataTypeStr', 'double', 'Position', [210 y+30 250 y+60]);
  add_block('simulink/Math Operations/Gain', [b '_x255'], 'Gain', '255', 'Position', [280 y+30 310 y+60]);
  add_block('simulink/Signal Routing/Mux', [b '_mux'], 'Inputs', '2', 'Position', [350 y 355 y+70]);
  add_block('simulink/Sinks/Scope', [b '_scope'], 'Position', [390 y+20 420 y+50]);
  L = @(a, c) add_line(mdl, [n a], [n c], 'autorouting', 'on');
  L('_cnt/1', '_cmp/1');  L('_OCR/1', '_cmp/2');  L('_cmp/1', '_dbl/1');
  L('_dbl/1', '_x255/1'); L('_cnt/1', '_mux/1');  L('_x255/1', '_mux/2');
  L('_mux/1', '_scope/1');
  fprintf('%s : %.3f ms -> %.1f Hz\n', n, T*1e3, 1/T);
end

save_system(mdl);
sim(mdl);
for k = 1:3, open_system([mdl '/' cfg{k,1} '_scope']); end
