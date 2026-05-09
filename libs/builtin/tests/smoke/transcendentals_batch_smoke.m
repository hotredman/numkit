clear

import compat.*

% Transcendentals + rounding batch — audit ТЗ closure 2026-05-09.
% atan2/atan2d + exp/expm1 + log/log2/log10/log1p +
% sqrt/hypot + floor/ceil/round/fix.

fprintf('atan2(1,1)   = %.15f  (expect pi/4)\n',   atan2(1,1));
fprintf('atan2d(1,-1) = %.15f  (expect 135)\n',    atan2d(1,-1));
fprintf('exp(1)       = %.15f  (expect 2.71828)\n', exp(1));
fprintf('expm1(1e-10) = %.15g  (precision win vs exp(1e-10)-1)\n', expm1(1e-10));
fprintf('log(e)       = %.15f  (expect 1)\n',      log(exp(1)));
fprintf('log2(8)      = %.15f  (expect 3)\n',      log2(8));
fprintf('log10(1000)  = %.15f  (expect 3)\n',      log10(1000));
fprintf('log1p(1e-10) = %.15g  (precision win)\n', log1p(1e-10));
fprintf('sqrt(2)      = %.15f  (expect 1.41421)\n', sqrt(2));
fprintf('hypot(3,4)   = %.15f  (expect 5)\n',      hypot(3,4));
fprintf('hypot(1e200,1e200) = %g  (no overflow)\n', hypot(1e200,1e200));
fprintf('floor(2.7)   = %g\n', floor(2.7));
fprintf('ceil(2.3)    = %g\n', ceil(2.3));
fprintf('round(0.5)   = %g  (half-away-from-zero)\n', round(0.5));
fprintf('fix(-2.7)    = %g  (toward zero)\n', fix(-2.7));
