clear

import compat.*

% interp1 — DEEP-PROBE 2026-05-31. Two MATLAB-parity fixes:
%  (a) MATRIX Y: interp1 interpolates DOWN each column of Y (size(Y,1)
%      must equal length(x)); output is length(xq) x size(Y,2),
%      regardless of xq orientation. Previously threw "x and y must
%      have same length".
%  (b) 'nearest' tie-break: an exactly-halfway query rounds UP to the
%      higher neighbor (MATLAB), e.g. interp1([1 2 3],...,2.5,'nearest').
% Reference: MATLAB R2025b.

x = [1 2 3];
Y = [10 100; 20 200; 30 300];

fprintf('=== matrix Y, linear, scalar xq=2.5 ===\n');
ml = interp1(x, Y, 2.5);
fprintf('ml = %g %g   (expect 25 250)  size %dx%d (expect 1x2)\n', ...
        ml(1), ml(2), size(ml,1), size(ml,2));

fprintf('\n=== matrix Y, linear, vector xq, NaN extrap ===\n');
mle = interp1(x, Y, [0 2.5 5]);
fprintf('size %dx%d (expect 3x2); mle(2,1)=%g mle(2,2)=%g (expect 25 250)\n', ...
        size(mle,1), size(mle,2), mle(2,1), mle(2,2));
fprintf('isnan(mle(1,1))=%d isnan(mle(3,2))=%d (expect 1 1)\n', ...
        isnan(mle(1,1)), isnan(mle(3,2)));

fprintf('\n=== matrix Y, nearest, extrapval -1 ===\n');
mn = interp1(x, Y, [0 2.5 5], 'nearest', -1);
fprintf('mn(1,1)=%g mn(2,1)=%g mn(3,2)=%g (expect -1 30 -1)\n', mn(1,1), mn(2,1), mn(3,2));

fprintf('\n=== matrix Y, spline (extrapolates) ===\n');
ms = interp1(x, Y, [0 2.5 5], 'spline');
fprintf('ms(1,1)=%g ms(3,1)=%g ms(2,2)=%g (expect 0 50 250)\n', ms(1,1), ms(3,1), ms(2,2));

fprintf('\n=== nearest tie-break rounds UP ===\n');
fprintf('interp1([1 2 3],[10 20 30],[2.4 2.5 2.6],''nearest'') = ');
nt = interp1([1 2 3],[10 20 30],[2.4 2.5 2.6],'nearest');
fprintf('%g %g %g   (expect 20 30 30)\n', nt(1), nt(2), nt(3));
