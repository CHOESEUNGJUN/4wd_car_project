% SPDX-License-Identifier: MIT · Copyright (c) 2026 4wd-car Team 2 "Gulno" (굴러라 노란바퀴) · see LICENSE, NOTICE.md
% pwm_shapes.m : 톱니파(Fast PWM)와 삼각파(Phase Correct PWM) 비교. 설명 : docs/PWM.md
% MATLAB 에서 열고 Run (F5).

clk = 16e6; duty = 0.30;              % UNO 시계 16MHz, 듀티 30%

%% 1) 장난감 TOP = 2 (칸 단위)
TOP = 2;
saw = repmat(0:TOP, 1, 4);                    % 톱니파 0 1 2 | 0 1 2 | ...
tri = repmat([0:TOP, TOP-1:-1:1], 1, 3);      % 삼각파 0 1 2 1 | 0 1 2 1 | ...
figure('Name', '장난감 TOP = 2');
subplot(2,1,1); stairs(0:numel(saw)-1, saw, 'b', 'LineWidth', 2); grid on; ylim([-0.5 2.5])
title('톱니파 : 한 주기 = TOP+1 = 3칸'); xlabel('칸'); ylabel('카운트')
subplot(2,1,2); stairs(0:numel(tri)-1, tri, 'r', 'LineWidth', 2); grid on; ylim([-0.5 2.5])
title('삼각파 : 한 주기 = 2×TOP = 4칸'); xlabel('칸'); ylabel('카운트')

%% 2) 실제 8비트 TOP = 255 (20ms)
TOP = 255; OCR = round(duty*TOP); tEnd = 20;
cfg = {'Timer0 기본 (톱니파, 분주 64)', 'saw', 64
       'Timer0 분주 256 (톱니파)',      'saw', 256
       'Timer1 우리 차 (삼각파, 256)',  'tri', 256};
figure('Name', '8비트 실제 타이머');
for k = 1:3
  [name, shape, pre] = cfg{k,:};
  if strcmp(shape, 'saw'), lap = 0:TOP; else, lap = [0:TOP, TOP-1:-1:1]; end
  dt = pre/clk*1e3; n = ceil(tEnd/dt);         % 한 칸 시간 (ms)
  cnt = lap(mod(0:n-1, numel(lap)) + 1); t = (0:n-1)*dt;
  subplot(3,1,k); hold on; grid on
  plot(t, cnt, 'Color', [.6 .6 .6]);
  yline(OCR, 'k--', '30% 선');
  area(t, (cnt < OCR)*TOP, 'FaceColor', 'g', 'FaceAlpha', .25, 'EdgeColor', 'none');   % 켜짐
  title(sprintf('%s : %d칸 → %.1f Hz', name, numel(lap), clk/(pre*numel(lap))));
  xlim([0 tEnd]); ylim([0 TOP+10]); xlabel('ms'); ylabel('카운트')
end

%% 3) 듀티 비교 : analogWrite 90, 100, 120 (Timer1 삼각파, 122.5Hz)
vals = [90 100 120];
lap = [0:TOP, TOP-1:-1:1]; dt = 256/clk*1e3; n = ceil(16.4/dt);
cnt = lap(mod(0:n-1, numel(lap)) + 1); t = (0:n-1)*dt;
figure('Name', '듀티 비교 90 / 100 / 120');
subplot(4,1,1); hold on; grid on; plot(t, cnt, 'k');
for v = vals, yline(v, '--', num2str(v)); end
ylim([0 265]); xlim([0 16.4]); ylabel('카운트'); title('카운트와 analogWrite 값 (가로 선)')
for k = 1:3
  on = cnt < vals(k);
  subplot(4,1,k+1); area(t, double(on), 'FaceColor', 'g', 'FaceAlpha', .5, 'EdgeColor', 'none'); grid on
  title(sprintf('analogWrite %d → 듀티 %.0f%% · 한 번 켜짐 %.2f ms', vals(k), vals(k)/255*100, vals(k)/255*numel(lap)*dt));
  yticks([]); xlim([0 16.4])
end
xlabel('ms')
