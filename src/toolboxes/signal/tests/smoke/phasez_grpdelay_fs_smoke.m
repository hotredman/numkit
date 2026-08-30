clear

% phasez / grpdelay sample-rate form fn(b,a,n,fs) (DEEP-PROBE 2026-05-31).
% Like freqz, when a sample rate fs is given the returned frequency vector
% is in Hz over [0, fs/2) = w*fs/(2*pi). The phase (phasez) and group delay
% in samples (grpdelay) are unchanged. numkit was ignoring fs and returning
% radians. vs MATLAB R2025b.

fprintf('=== phasez([1 1],[1 -0.5], 4, 100) ===\n');
[ph, f] = phasez([1 1], [1 -0.5], 4, 100);
fprintf('f  = [%.4g %.4g %.4g %.4g]  (expect [0 12.5 25 37.5] Hz)\n', f(1), f(2), f(3), f(4));
fprintf('ph(2) = %.8f  (expect -0.89317312, unchanged by fs)\n', ph(2));

fprintf('\n=== grpdelay([1 1],[1 -0.5], 4, 100) ===\n');
[gd, fg] = grpdelay([1 1], [1 -0.5], 4, 100);
fprintf('f  = [%.4g %.4g %.4g %.4g]  (expect [0 12.5 25 37.5] Hz)\n', fg(1), fg(2), fg(3), fg(4));
fprintf('gd(2) = %.8f  (expect 0.69074357 samples, unchanged by fs)\n', gd(2));

fprintf('\n=== no fs: frequency stays in radians [0, pi) ===\n');
[~, w] = grpdelay([1 1], [1 -0.5], 4);
fprintf('w = [%.5f %.5f %.5f %.5f]  (expect [0 0.78540 1.57080 2.35619] rad)\n', w(1), w(2), w(3), w(4));
