clear

% stepinfo now returns MATLAB R2025b's full 9-field struct. numkit previously
% returned only 8 fields -- the TransientTime field (the 2nd field, between
% RiseTime and SettlingTime) was missing.
%
% TransientTime = the last time the error |y(t)-yfinal| exceeds 2% of the PEAK
% deviation max|y(t)-yfinal|.   SettlingTime uses 2% of |yinit-yfinal|=|yfinal|.
% For a standard step (yinit=0) the peak deviation occurs at t=0 and equals
% |yfinal|, so TransientTime == SettlingTime. MATLAB R2025b field order:
%   RiseTime TransientTime SettlingTime SettlingMin SettlingMax ...
%   Overshoot Undershoot Peak PeakTime

fprintf('--- stepinfo field set ---\n');
info = stepinfo(tf(1, [1 1]));
fn = fieldnames(info);
fprintf('nf=%d  (expect 9)\n', numel(fn));
fprintf('fields:');
for i = 1:numel(fn), fprintf(' %s', fn{i}); end
fprintf('\n');
fprintf('field #2 = %s   (expect TransientTime)\n', fn{2});

fprintf('--- 1st-order  tf(1,[1 1]) ---\n');
fprintf('RiseTime=%.6f  TransientTime=%.6f  SettlingTime=%.6f  eq=%d\n', ...
        info.RiseTime, info.TransientTime, info.SettlingTime, ...
        double(info.TransientTime == info.SettlingTime));

fprintf('--- 2nd-order underdamped  tf(1,[1 0.4 1]) ---\n');
i2 = stepinfo(tf(1, [1 0.4 1]));
fprintf('Overshoot=%.4f%%  TransientTime=%.6f  SettlingTime=%.6f  eq=%d\n', ...
        i2.Overshoot, i2.TransientTime, i2.SettlingTime, ...
        double(i2.TransientTime == i2.SettlingTime));
fprintf('(expect Overshoot>0; TransientTime==SettlingTime)\n');
