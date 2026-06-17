clear

import compat.*

% rot90 on non-DOUBLE matrices — DEEP-PROBE 2026-05-31. CELL/STRING were
% handled earlier via rot90CellStr, but the 2-D/3-D POD path (CHAR / LOGICAL /
% COMPLEX / single / int) was still DOUBLE-only and threw "Not a double array".
% rot90 is a pure rearrangement, so it is type-preserving (reuses the existing
% byte-copy kernels). Reference: MATLAB R2025b.

fprintf('=== char matrix ===\n');
cm = rot90(['ab';'cd']);
fprintf('rot90 90 -> [%s;%s]  (expect bd;ac)\n', cm(1,:), cm(2,:));
cm2 = rot90(['ab';'cd'], 2);
fprintf('rot90 180 -> [%s;%s]  (expect dc;ba)\n', cm2(1,:), cm2(2,:));
cm3 = rot90(['ab';'cd'], 3);
fprintf('rot90 270 -> [%s;%s]  (expect ca;db)\n', cm3(1,:), cm3(2,:));

fprintf('\n=== logical / complex / single ===\n');
fprintf('rot90 logical [1 0;0 0] -> %s  (expect [0 0;1 0])\n', mat2str(double(rot90(logical([1 0;0 0])))));
zx = rot90([1+1i 2; 3 4]);
fprintf('rot90 complex (1,1) = %g+%gi  (expect 2+0i)\n', real(zx(1,1)), imag(zx(1,1)));
sg = rot90(single([1 2;3 4]));
fprintf('rot90 single -> %s (class %s)  (expect [2 4;1 3], single)\n', mat2str(double(sg)), class(sg));

fprintf('\n=== cell still OK + double unchanged ===\n');
cc = rot90({1,2;3,4});
fprintf('rot90 cell (1,1) = %g  (expect 2)\n', cc{1,1});
disp(rot90([1 2 3;4 5 6]));   % expect [3 6;2 5;1 4]
