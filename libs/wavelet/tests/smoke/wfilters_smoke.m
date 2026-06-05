clear;
import compat.*;

% wfilters — wavelet filter quadruple. Closes
%
%
% CRITICAL fix 2026-05-08:
%   - Lo_D / Lo_R labels were SWAPPED relative to MATLAB R2025b (root
%     cause of the dwt / wavedec / waverec value mismatch).
%   - 1-output 'd'/'r'/'l'/'h' form now returns a 2×Lf matrix
%     (was returning two separate row vectors).
%   - Cascade fix: dwt now matches MATLAB byte-for-byte on the
%     analysis filters; round-trip preserved at ≤ 1e-10.

fprintf('--- wfilters(''db2'') ---\n');
[Lo_D, Hi_D, Lo_R, Hi_R] = wfilters('db2');
fprintf('Lo_D = '); disp(Lo_D);
fprintf('expect [-0.1294 0.2241 0.8365 0.4830]\n');
fprintf('Lo_R = '); disp(Lo_R);
fprintf('expect [0.4830 0.8365 0.2241 -0.1294]\n');
fprintf('Hi_D = '); disp(Hi_D);
fprintf('expect [-0.4830 0.8365 -0.2241 -0.1294]\n');
fprintf('Hi_R = '); disp(Hi_R);
fprintf('expect [-0.1294 -0.2241 0.8365 -0.4830]\n\n');

fprintf('--- 1-output 2×Lf form ---\n');
Fd = wfilters('db2', 'd');
fprintf('size(Fd) = [%d %d] (expect [2 4])\n', size(Fd, 1), size(Fd, 2));
disp(Fd);
fprintf('row 1 = Lo_D, row 2 = Hi_D\n\n');

fprintf('--- dwt cascade vs MATLAB ---\n');
x = (1:8)';
[cA, cD] = dwt(x, 'db2');
fprintf('cA = '); disp(cA);
fprintf('expect [1.7678 2.3108 5.1392 7.9676 10.9602]\n');
fprintf('cD = '); disp(cD);
fprintf('expect [-0.6124 ~0 ~0 ~0 +0.6124]\n\n');

fprintf('--- round-trip preserved ---\n');
xr = idwt(cA, cD, 'db2');
fprintf('idwt round-trip max diff = %.3e\n', max(abs(xr(:) - x(:))));

fprintf('--- multi-level wavedec/waverec ---\n');
xm = (1:16)';
[c, l] = wavedec(xm, 3, 'db2');
fprintf('c first 6 = '); disp(c(1:6)');
fprintf('expect [3.8833 3.6259 21.4103 42.7563 -0.1320 -1.3976]\n');
fprintf('l = '); disp(l');
fprintf('expect [4 4 6 9 16]\n');
xrm = waverec(c, l, 'db2');
fprintf('waverec round-trip max diff = %.3e\n', max(abs(xrm(:) - xm(:))));
