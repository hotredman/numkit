clear
import compat.*

% spectrogram's f and t output axes now honour fs (MATLAB R2025b):
%   with fs:  f = k*fs/nfft  (Hz),       t = segment_centre/fs  (seconds)
%   no  fs:   normalized fs = 2*pi, so   f in [0, pi],  t = centre/(2*pi)
% Previously f was always 0..pi and t was in raw samples regardless of fs.

x = sin(2*pi*0.1*(0:99)) + 0.5*sin(2*pi*0.25*(0:99));

[s, f, t] = spectrogram(x, 16, 8, 16, 100);
fprintf('WITH fs=100, nfft=16:\n');
fprintf('  f(2)=%.6f f(end)=%.6f  (expect 6.25, 50)\n', f(2), f(end));
fprintf('  t(1)=%.6f t(2)=%.6f    (expect 0.08, 0.16)\n', t(1), t(2));
fprintf('  size(s)=%dx%d (unchanged by fs)\n', size(s,1), size(s,2));

[s0, f0, t0] = spectrogram(x, 16, 8, 16);
fprintf('NO fs (normalized, fs=2*pi):\n');
fprintf('  f0(end)=%.6f  (expect pi=3.141593)\n', f0(end));
fprintf('  t0(1)=%.6f t0(2)=%.6f  (expect 1.273240, 2.546479 = 8/(2pi), 16/(2pi))\n', t0(1), t0(2));
