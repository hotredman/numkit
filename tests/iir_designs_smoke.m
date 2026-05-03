import compat.*

% cheby1(4, 0.5, 0.4) — digital lowpass, normalised cutoff 0.4 (× Nyquist).
% MATLAB R2025b reference:
%   b = [0.0186  0.0744  0.1115  0.0744  0.0186]
%   a = [1.0000 -1.7556  1.7505 -0.8780  0.1893]
[b, a] = cheby1(4, 0.5, 0.4);
fprintf('cheby1(4, 0.5, 0.4):\n');
fprintf('  b = ['); for i = 1:length(b); fprintf('%.4f ', b(i)); end; fprintf(']\n');
fprintf('  a = ['); for i = 1:length(a); fprintf('%.4f ', a(i)); end; fprintf(']\n');

% Sum of b should equal sum of a at DC (frequency 0): for lowpass, |H(0)| ≈ 1
% (cheby1 has -Rp dB ripple at the passband edge, but DC is at 1 for even N
%  and 1/√(1+ε²) for odd N).
hf0 = sum(b) / sum(a);
fprintf('  H(0) = %.4f  (expect ≈ 1 for lowpass)\n', hf0);

% cheby2 — stopband 30 dB, similar shape
[b2, a2] = cheby2(4, 30, 0.4);
fprintf('cheby2(4, 30, 0.4):\n');
fprintf('  b = ['); for i = 1:length(b2); fprintf('%.4f ', b2(i)); end; fprintf(']\n');
fprintf('  a = ['); for i = 1:length(a2); fprintf('%.4f ', a2(i)); end; fprintf(']\n');
fprintf('  H(0) = %.4f  (expect ≈ 1)\n', sum(b2)/sum(a2));

% besself analog (Bessel is naturally analog)
[bb, ab] = besself(3, 1, 'low', 's');
fprintf('besself(3, 1, ''low'', ''s''):\n');
fprintf('  b = ['); for i = 1:length(bb); fprintf('%.4f ', bb(i)); end; fprintf(']\n');
fprintf('  a = ['); for i = 1:length(ab); fprintf('%.4f ', ab(i)); end; fprintf(']\n');
% At s=0: H(0) = b(end)/a(end). MATLAB: for besself(3, 1) prototype, k=1
% so H(0) should equal 1.
fprintf('  H(0) = %.4f  (expect ≈ 1)\n', bb(end)/ab(end));

% Bandpass cheby1
[b3, a3] = cheby1(2, 0.5, [0.3 0.6]);
fprintf('cheby1(2, 0.5, [0.3 0.6]):\n');
fprintf('  length(b) = %d  (expect 5 for N=2 bandpass)\n', length(b3));
fprintf('  length(a) = %d  (expect 5)\n', length(a3));
% At DC and Nyquist, magnitude should be small
hf0 = sum(b3) / sum(a3);
hfN = sum(b3 .* (-1).^(0:length(b3)-1)) / sum(a3 .* (-1).^(0:length(a3)-1));
fprintf('  |H(0)|     = %.4f  (small expected for bandpass)\n', abs(hf0));
fprintf('  |H(Nyq)|   = %.4f  (small expected for bandpass)\n', abs(hfN));
