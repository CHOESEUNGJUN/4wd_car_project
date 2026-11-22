% SPDX-License-Identifier: MIT · Copyright (c) 2026 4wd-car Team 2 "Gulno" (굴러라 노란바퀴) · see LICENSE, NOTICE.md
% pwm_power_simulink.m : 같은 analogWrite 값에서 모터 코일에 실제로 흐르는 전류 비교. 설명 : docs/PWM.md 3장
%   원래 (버전 C, 5·6번 핀 = Timer0 톱니파, 분주 64)  976.6Hz
%   우리 (버전 E, 9·10번 핀 = Timer1 삼각파, 분주 256) 122.5Hz
% 모델 (간단한 RL, 바퀴가 서 있을 때 = 역기전력 0) :
%   켜짐 : 코일에 +V,  꺼짐 : L298N 이 끊으면 다이오드로 -V 가 걸려 전류가 빨리 0 으로 (0 아래로는 안 감)
%   L di/dt = u - R i
% 결과 : 스코프 하나, 값마다 한 칸 (노랑 976.6Hz, 파랑 122.5Hz). 같은 그림을 MATLAB figure 로도 그리고, 명령 창에 평균·최대 전류 표
% 문서 그림 저장 : export_doc_figures.m 이 lang, exportFile 을 정하고 이 파일을 부름

if ~exist('vals', 'var'), vals = [30 60 90 120 130 150]; end   % analogWrite 값. 명령 창에서 vals = [60 120 200]; 처럼 먼저 정해도 됨
V  = 7.2;                   % 배터리 전압 : 1.2V 충전지 6개 (L298N 전압 강하는 무시)
R  = 5;                     % 모터 코일 저항 (옴), 예시값
Lh = 1.5e-3;                % 모터 코일 인덕턴스 (H), 예시값 -> 시간상수 L/R = 0.3ms
tEnd = 0.04;                % 40ms
if ~exist('lang', 'var'), lang = 'ko'; end

clk = 16e6; TOP = 255; nv = numel(vals);
cfg = {'C_976Hz', 'saw', 64
       'E_122Hz', 'tri', 256};

mdl = 'pwm_power';
if bdIsLoaded(mdl), close_system(mdl, 0); end
new_system(mdl); if usejava('desktop'), open_system(mdl); end
set_param(mdl, 'StopTime', num2str(tEnd), 'MaxStep', '1e-6');
add_block('simulink/Sinks/Scope', [mdl '/I_scope'], 'NumInputPorts', num2str(nv), 'Position', [900 300 940 300+40*nv]);
sc = get_param([mdl '/I_scope'], 'ScopeConfiguration');
sc.LayoutDimensions = [nv 1];                             % 값마다 한 칸 (위부터 vals 순서)

for k = 1:nv
  v = vals(k); y0 = 40 + (k-1)*260;
  add_block('simulink/Signal Routing/Mux', sprintf('%s/I_%d_mux', mdl, v), 'Inputs', '2', 'Position', [820 y0+60 825 y0+160]);
  add_line(mdl, sprintf('I_%d_mux/1', v), sprintf('I_scope/%d', k), 'autorouting', 'on');
  for j = 1:2
    [tag, shape, pre] = cfg{j, :};
    if strcmp(shape, 'saw'), T = (TOP+1)*pre/clk; ts = sprintf('[0 %g]', T); ys = sprintf('[0 %d]', TOP);
    else, T = 2*TOP*pre/clk; ts = sprintf('[0 %g %g]', T/2, T); ys = sprintf('[0 %d 0]', TOP); end
    n = sprintf('%s_%d', tag, v); b = [mdl '/' n]; y = y0 + (j-1)*120;
    add_block('simulink/Sources/Repeating Sequence', [b '_cnt'], 'rep_seq_t', ts, 'rep_seq_y', ys, 'Position', [20 y 80 y+30]);
    add_block('simulink/Sources/Constant', [b '_analogWrite'], 'Value', num2str(v), 'Position', [20 y+50 80 y+80]);
    add_block('simulink/Logic and Bit Operations/Relational Operator', [b '_cmp'], 'Operator', '<', 'Position', [120 y+20 150 y+60]);
    add_block('simulink/Signal Attributes/Data Type Conversion', [b '_dbl'], 'OutDataTypeStr', 'double', 'Position', [180 y+25 220 y+55]);
    add_block('simulink/Math Operations/Gain', [b '_2V'], 'Gain', num2str(2*V), 'Position', [250 y+25 290 y+55]);
    add_block('simulink/Math Operations/Bias', [b '_u'], 'Bias', num2str(-V), 'Position', [320 y+25 360 y+55]);   % 켜짐 +V, 꺼짐 -V
    add_block('simulink/Math Operations/Sum', [b '_sum'], 'Inputs', '+-', 'Position', [390 y+25 410 y+55]);
    add_block('simulink/Math Operations/Gain', [b '_invL'], 'Gain', num2str(1/Lh), 'Position', [440 y+25 480 y+55]);
    add_block('simulink/Continuous/Integrator', [b '_i'], 'LimitOutput', 'on', 'LowerSaturationLimit', '0', 'Position', [510 y+25 540 y+55]);
    add_block('simulink/Math Operations/Gain', [b '_R'], 'Gain', num2str(R), 'Orientation', 'left', 'Position', [440 y+75 480 y+100]);
    add_block('simulink/Sinks/To Workspace', [b '_log'], 'VariableName', n, 'SaveFormat', 'Timeseries', 'Position', [600 y+70 660 y+95]);
    L = @(a, c) add_line(mdl, [n a], [n c], 'autorouting', 'on');
    L('_cnt/1', '_cmp/1'); L('_analogWrite/1', '_cmp/2'); L('_cmp/1', '_dbl/1'); L('_dbl/1', '_2V/1');
    L('_2V/1', '_u/1'); L('_u/1', '_sum/1'); L('_sum/1', '_invL/1'); L('_invL/1', '_i/1');
    L('_i/1', '_R/1'); L('_R/1', '_sum/2'); L('_i/1', '_log/1');
    add_line(mdl, [n '_i/1'], sprintf('I_%d_mux/%d', v, j), 'autorouting', 'on');
  end
end

out = sim(mdl);

% --- 평균·최대 전류 ---
Imax = V/R;                         % 계속 켜 두었을 때의 전류
win = tEnd - 3*2*TOP*256/clk;       % 마지막 3주기(122.5Hz 기준)로 평균
r = zeros(nv, 2, 2);                % (값, 주파수, [평균 최대])
for k = 1:nv
  for j = 1:2
    ts = out.get(sprintf('%s_%d', cfg{j,1}, vals(k))); t = ts.Time; i = ts.Data(:); m = t >= win;
    r(k, j, :) = [trapz(t(m), i(m)) / (t(find(m, 1, 'last')) - t(find(m, 1))), max(i(m))];
  end
end
fprintf('\n코일 전류 (계속 켜 두면 %.2f A). 평균 = 평균 힘, 최대 = 순간 힘\n', Imax);
fprintf('analogWrite | 듀티 | 976.6Hz 평균 / 최대        | 122.5Hz 평균 / 최대\n');
for k = 1:nv
  fprintf('%11d | %3.0f%% | %.2f A (%3.0f%%) / %.2f A (%3.0f%%) | %.2f A (%3.0f%%) / %.2f A (%3.0f%%)\n', ...
    vals(k), vals(k)/TOP*100, r(k,1,1), r(k,1,1)/Imax*100, r(k,1,2), r(k,1,2)/Imax*100, r(k,2,1), r(k,2,1)/Imax*100, r(k,2,2), r(k,2,2)/Imax*100);
end

% --- 같은 결과를 figure 로 (값마다 한 칸) ---
if strcmp(lang, 'en')
  lg = {'976.6 Hz (original, Timer0 pins 5·6)', '122.5 Hz (ours, Timer1 pins 9·10)'};
  ttl = 'analogWrite %d (duty %.0f%%): mean %.2f A vs %.2f A, peak %.2f A vs %.2f A';
  yl = 'coil current (A)'; xl = 'time (ms)';
  sup = sprintf('Coil current with the wheel stopped (RL model: %.1f V, %g Ω, %.1f mH)', V, R, Lh*1e3);
else
  lg = {'976.6Hz (원래, Timer0 5·6번 핀)', '122.5Hz (우리, Timer1 9·10번 핀)'};
  ttl = 'analogWrite %d (듀티 %.0f%%) : 평균 %.2f A vs %.2f A, 최대 %.2f A vs %.2f A';
  yl = '코일 전류 (A)'; xl = '시간 (ms)';
  sup = sprintf('바퀴가 서 있을 때 코일 전류 (RL 모델 : %.1fV, %gΩ, %.1fmH)', V, R, Lh*1e3);
end
cols = [0.93 0.69 0.13; 0 0.45 0.74];   % 노랑 976.6Hz, 파랑 122.5Hz (스코프와 같은 색)
f = figure('Name', 'pwm_power', 'Position', [100 100 1100 260*nv], 'Color', 'w');
tl = tiledlayout(f, nv, 1, 'TileSpacing', 'compact');
for k = 1:nv
  nexttile; hold on; grid on
  for j = 1:2
    ts = out.get(sprintf('%s_%d', cfg{j,1}, vals(k)));
    plot(ts.Time*1e3, ts.Data(:), 'LineWidth', 1.4, 'Color', cols(j, :));
  end
  yline(Imax, ':', sprintf('%.2f A', Imax));
  ylim([0 2.1]); xlim([0 tEnd*1e3]); ylabel(yl)
  title(sprintf(ttl, vals(k), vals(k)/TOP*100, r(k,1,1), r(k,2,1), r(k,1,2), r(k,2,2)));
  if k == 1, legend(lg, 'Location', 'north', 'Orientation', 'horizontal'); end
end
xlabel(tl, xl); title(tl, sup);
if exist('exportFile', 'var'), exportgraphics(f, exportFile, 'Resolution', 110); end
if usejava('desktop'), open_system([mdl '/I_scope']); end
