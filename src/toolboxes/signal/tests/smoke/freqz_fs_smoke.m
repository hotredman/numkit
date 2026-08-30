clear

% freqz sample-rate form freqz(b,a,n,fs) (DEEP-PROBE 2026-05-31). When a
% sample rate fs is given, MATLAB returns the frequency vector in Hz over
% [0, fs/2) (or [0, fs) with 'whole'), i.e. f = w*fs/(2*pi). The response H
% is unchanged. numkit was ignoring fs and returning radians. vs MATLAB R2025b.

fprintf('=== freqz([1 1],[1 -0.5], 4, 100) ===\n');
[h, f] = freqz([1 1], [1 -0.5], 4, 100);
fprintf('f = [%.4g %.4g %.4g %.4g]  (expect [0 12.5 25 37.5] Hz, NOT radians)\n', f(1), f(2), f(3), f(4));
fprintf('|h(1)| = %.4g  (DC gain = (1+1)/(1-0.5) = 4, unchanged by fs)\n', abs(h(1)));

fprintf('\n=== whole + fs spans [0, fs) ===\n');
[hw, fw] = freqz([1 1], [1 -0.5], 4, 'whole', 200);
fprintf('f = [%.4g %.4g %.4g %.4g]  (expect [0 50 100 150] Hz)\n', fw(1), fw(2), fw(3), fw(4));

fprintf('\n=== no fs: frequency stays in radians [0, pi) ===\n');
[h2, w2] = freqz([1 1], [1 -0.5], 4);
fprintf('w = [%.5f %.5f %.5f %.5f]  (expect [0 0.78540 1.57080 2.35619] rad)\n', w2(1), w2(2), w2(3), w2(4));
