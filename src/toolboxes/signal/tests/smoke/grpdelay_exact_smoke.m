clear

% grpdelay exact group delay (DEEP-PROBE 2026-05-31). numkit computed the
% group delay by finite-differencing the unwrapped phase, which is wildly
% inaccurate at small npts (and can't represent negative group delay). It
% now uses MATLAB's exact ramped-polynomial method:
%   c = conv(b, reverse(a));  gd(w) = Re{CR(e^jw)/C(e^jw)} - (na-1)
% with CR[n] = n*c[n]. vs MATLAB R2025b.

fprintf('=== H = (1 + z^-1)/(1 - 0.5 z^-1), n = 4 ===\n');
[gd, w] = grpdelay([1 1], [1 -0.5], 4);
fprintf('gd = [%.6f %.6f %.6f %.6f]\n', gd(1), gd(2), gd(3), gd(4));
fprintf('     (expect [1.500000 0.690744 0.300000 0.191609])\n');
fprintf('gd(0)=%.3f  (sym FIR num delay 0.5 + pole-at-0.5 +1 = 1.5)\n', gd(1));

fprintf('\n=== filter with NEGATIVE group delay, n = 5 ===\n');
[gd2, w2] = grpdelay([1 -0.3 0.2], [1 0.4 0.1], 5);
fprintf('gd = [%.6f %.6f %.6f %.6f %.6f]\n', gd2(1), gd2(2), gd2(3), gd2(4), gd2(5));
fprintf('     (expect [-0.288889 -0.481974 -0.656839 0.188404 0.715502])\n');

fprintf('\n=== sanity: pure delay z^-3 -> gd == 3 everywhere ===\n');
[gd3, w3] = grpdelay([0 0 0 1], 1, 6);
fprintf('gd = [%.3f %.3f %.3f %.3f %.3f %.3f]  (expect all 3)\n', ...
        gd3(1), gd3(2), gd3(3), gd3(4), gd3(5), gd3(6));
