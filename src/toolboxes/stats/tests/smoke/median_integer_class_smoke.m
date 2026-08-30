clear

% median preserves the integer class (round half-away-from-zero + saturate).
% Integer class preservation added 2026-05-30 (DEEP-PROBE). vs MATLAB R2025b.

fprintf('=== integer median: class preserved, half-away rounding ===\n');
mi = median(int32([1 2 3 4]));
fprintf('median(int32([1 2 3 4])) = %g class=%s (expect 3 int32)\n', double(mi), class(mi));
mo = median(int32([1 2 3]));
fprintf('median(int32([1 2 3]))   = %g class=%s (expect 2 int32)\n', double(mo), class(mo));
mu = median(uint8([10 20 30 41]));
fprintf('median(uint8([10 20 30 41])) = %g class=%s (expect 25 uint8)\n', double(mu), class(mu));

fprintf('\n=== negative half-away rounding ===\n');
mn = median(int8([-1 -2]));
fprintf('median(int8([-1 -2]))  = %g class=%s (expect -2 int8)\n', double(mn), class(mn));
mn2 = median(int8([-2 -3]));
fprintf('median(int8([-2 -3]))  = %g class=%s (expect -3 int8)\n', double(mn2), class(mn2));

fprintf('\n=== per-column matrix keeps class ===\n');
mc = median(int32([1 2; 3 4; 5 6]));
fprintf('median(int32 3x2) cols = %g %g class=%s (expect 3 4 int32)\n', double(mc(1)), double(mc(2)), class(mc));

fprintf('\n=== double / single unchanged (regress) ===\n');
fprintf('median([1 2 3 4]) = %g class=%s (expect 2.5 double)\n', median([1 2 3 4]), class(median([1 2 3 4])));
fprintf('median(single([1 2 3 4])) class=%s (expect single)\n', class(median(single([1 2 3 4]))));
