% SPDX-License-Identifier: MIT · Copyright (c) 2026 4wd-car Team 2 "Gulno" (굴러라 노란바퀴) · see LICENSE, NOTICE.md
% export_doc_figures.m : docs/PWM.md 의 그림(docs/img/pwm_*.png, 한국어·영어)을 MATLAB 으로 다시 저장한다
out = fullfile(fileparts(mfilename('fullpath')), '..', 'img');
clk = 16e6; TOP = 255;
L.ko = struct('saw','톱니파 (Fast PWM, Timer0) : 한 주기 = TOP+1 = 3칸', ...
  'tri','삼각파 (Phase Correct PWM, Timer1·2) : 한 주기 = 2×TOP = 4칸', 'step','칸', 'cnt','카운트', 'ms','시간 (ms)', ...
  'rows',{{'Timer0 기본 · 톱니파 · 분주 64','Timer0 분주 256 · 톱니파 (쓰지 않음)','Timer1 · 삼각파 · 분주 256 (우리 차)'}}, ...
  'fmt','%s : 16,000,000 ÷ %d ÷ %d = %.1f Hz · 한 번 켜짐 %.2f ms', 'line','30%% 선 (77)', ...
  'dl',{{'왼쪽 바퀴 120+30 = 150 → 59%','직진 BASE 120 → 47%','오른쪽 바퀴 120-30 = 90 → 35%'}}, ...
  'dsup','듀티 = analogWrite 값 ÷ 255 (Timer1 삼각파, 122.5 Hz)');
L.en = struct('saw','Sawtooth (Fast PWM, Timer0): one period = TOP+1 = 3 steps', ...
  'tri','Triangle (Phase Correct PWM, Timer1·2): one period = 2×TOP = 4 steps', 'step','step', 'cnt','count', 'ms','time (ms)', ...
  'rows',{{'Timer0 default · sawtooth · prescaler 64','Timer0 prescaler 256 · sawtooth (not used)','Timer1 · triangle · prescaler 256 (our car)'}}, ...
  'fmt','%s: 16,000,000 ÷ %d ÷ %d = %.1f Hz · on for %.2f ms', 'line','30%% line (77)', ...
  'dl',{{'left wheel 120+30 = 150 → 59%','straight BASE 120 → 47%','right wheel 120-30 = 90 → 35%'}}, ...
  'dsup','duty = analogWrite value ÷ 255 (Timer1 triangle, 122.5 Hz)');
for lang = ["ko" "en"]
  t = L.(lang); sfx = ""; if lang == "en", sfx = ".en"; end
  % 1) TOP = 2
  f = figure('Visible','off','Position',[0 0 1100 560]);
  saw = repmat(0:2, 1, 5); saw = saw(1:13);
  tri = repmat([0 1 2 1], 1, 4); tri = tri(1:13);
  subplot(2,1,1); stairs(0:12, saw, 'b', 'LineWidth', 2); hold on; plot(0:12, saw, 'bo'); xline(0:3:12, '--');
  grid on; ylim([-0.5 2.5]); title(t.saw); xlabel(t.step); ylabel(t.cnt)
  subplot(2,1,2); stairs(0:12, tri, 'r', 'LineWidth', 2); hold on; plot(0:12, tri, 'ro'); xline(0:4:12, '--');
  grid on; ylim([-0.5 2.5]); title(t.tri); xlabel(t.step); ylabel(t.cnt)
  exportgraphics(f, fullfile(out, "pwm_counter_shapes" + sfx + ".png"), 'Resolution', 110); close(f)
  % 2) three timers
  f = figure('Visible','off','Position',[0 0 1100 800]);
  cfg = {'saw',64,[0.75 0.22 0.17]; 'saw',256,[0.84 0.54 0.06]; 'tri',256,[0.12 0.44 0.70]};
  for k = 1:3
    [shape, pre, col] = cfg{k,:};
    if strcmp(shape,'saw'), lap = 0:TOP; else, lap = [0:TOP, TOP-1:-1:1]; end
    dt = pre/clk*1e3; n = ceil(20/dt); cnt = lap(mod(0:n-1, numel(lap)) + 1); tt = (0:n-1)*dt;
    subplot(3,1,k); hold on; grid on
    area(tt, (cnt < 77)*TOP, 'FaceColor', col, 'FaceAlpha', .3, 'EdgeColor', 'none');
    plot(tt, cnt, 'Color', [.4 .4 .4]); yline(77, 'k--', sprintf(t.line));
    title(sprintf(t.fmt, t.rows{k}, pre, numel(lap), clk/(pre*numel(lap)), 0.3*numel(lap)*dt), 'Color', col);
    xlim([0 20]); ylim([0 265]); yticks([0 77 255]); ylabel(t.cnt)
  end
  xlabel(t.ms)
  exportgraphics(f, fullfile(out, "pwm_three_timers" + sfx + ".png"), 'Resolution', 110); close(f)
  % 3) duty lines
  f = figure('Visible','off','Position',[0 0 1100 700]);
  lap = [0:TOP, TOP-1:-1:1]; dt = 256/clk*1e3; n = ceil(16.4/dt);
  cnt = lap(mod(0:n-1, numel(lap)) + 1); tt = (0:n-1)*dt;
  v = [150 120 90]; cols = {[0.75 0.22 0.17], [.5 .5 .5], [0.12 0.44 0.70]};
  subplot(4,1,1:2); hold on; grid on; plot(tt, cnt, 'k');
  for k = 1:3, yline(v(k), '--', num2str(v(k)), 'Color', cols{k}, 'LineWidth', 1.3); end
  ylim([0 265]); yticks([0 90 120 150 255]); ylabel(t.cnt); title(t.dsup); xlim([0 16.4])
  for k = 1:3
    subplot(8,1,5+k); area(tt, double(cnt < v(k)), 'FaceColor', cols{k}, 'FaceAlpha', .6, 'EdgeColor', 'none');
    yticks([]); xlim([0 16.4]); title(t.dl{k}, 'Color', cols{k})
  end
  xlabel(t.ms)
  exportgraphics(f, fullfile(out, "pwm_duty_line" + sfx + ".png"), 'Resolution', 110); close(f)
end

% 듀티 90 / 100 / 120
clk = 16e6; TOP = 255; vals = [90 100 120];
lap = [0:TOP, TOP-1:-1:1]; dt = 256/clk*1e3; n = ceil(16.4/dt);
cnt = lap(mod(0:n-1, numel(lap)) + 1); tt = (0:n-1)*dt;
cols = {[0.12 0.44 0.70], [0.84 0.54 0.06], [0.75 0.22 0.17]};
T.ko = {'카운트와 analogWrite 값 (가로 선), Timer1 삼각파 122.5 Hz', 'analogWrite %d → 듀티 %.0f%% · 한 번 켜짐 %.2f ms', '카운트', '시간 (ms)'};
T.en = {'Count and analogWrite value (the line), Timer1 triangle 122.5 Hz', 'analogWrite %d → duty %.0f%% · on for %.2f ms', 'count', 'time (ms)'};
for lang = ["ko" "en"]
  t = T.(lang); sfx = ""; if lang == "en", sfx = ".en"; end
  f = figure('Visible','off','Position',[0 0 1100 700]);
  subplot(5,1,1:2); hold on; grid on; plot(tt, cnt, 'k');
  for k = 1:3, yline(vals(k), '--', num2str(vals(k)), 'Color', cols{k}, 'LineWidth', 1.3); end
  ylim([0 265]); yticks([0 90 100 120 255]); xlim([0 16.4]); ylabel(t{3}); title(t{1})
  for k = 1:3
    subplot(5,1,2+k); area(tt, double(cnt < vals(k)), 'FaceColor', cols{k}, 'FaceAlpha', .6, 'EdgeColor', 'none'); grid on
    title(sprintf(t{2}, vals(k), vals(k)/255*100, vals(k)/255*numel(lap)*dt), 'Color', cols{k}); yticks([]); xlim([0 16.4])
  end
  xlabel(t{4})
  exportgraphics(f, fullfile(out, "pwm_duty_90_100_120" + sfx + ".png"), 'Resolution', 110); close(f)
end
