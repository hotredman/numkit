clear
import compat.*

% Exact Durbin-Watson p-value (Imhof). Fixed 2026-06-05
% (bugs/stats/dwtest-pvalue.md). Reference: MATLAB R2025b.

r1 = [1 2 1 3 2 4]'; X1 = [ones(6,1) (1:6)'];
[p1, dw1] = dwtest(r1, X1);
fprintf('low DW : dw=%.7f both p=%.6g  (expect 0.3142857, ~0)\n', dw1, p1);
fprintf('   right=%.6g left=%.6g  (expect 0, 1)\n', ...
        dwtest(r1, X1, 'Tail', 'right'), dwtest(r1, X1, 'Tail', 'left'));

r3 = [1 -1 1 -1 1 -1 1 -1]'; X3 = ones(8,1);
[p3, dw3] = dwtest(r3, X3);
fprintf('high DW: dw=%.4f both p=%.8g  (expect 3.5, 0.00569352)\n', dw3, p3);
fprintf('   right=%.8g left=%.8g  (expect 0.99715324, 0.00284676)\n', ...
        dwtest(r3, X3, 'Tail', 'right'), dwtest(r3, X3, 'Tail', 'left'));

r4 = [2 1 4 3 6 5 8 7]'; X4 = [ones(8,1) (1:8)'];
[p4, dw4] = dwtest(r4, X4);
fprintf('design2: dw=%.7f both p=%.6g  (expect 0.1519608, ~0)\n', dw4, p4);
