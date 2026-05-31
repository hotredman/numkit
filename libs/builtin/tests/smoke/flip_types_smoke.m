clear

import compat.*

% fliplr / flipud on non-DOUBLE matrices — DEEP-PROBE 2026-05-31. CELL and
% STRING were handled earlier, but the 2-D POD path (CHAR / LOGICAL / COMPLEX /
% single / int matrices) was still DOUBLE-only and threw "Not a double array".
% flip is a pure rearrangement, so it is type-preserving. Reference: MATLAB
% R2025b.

fprintf('=== char matrix ===\n');
cm = fliplr(['ab';'cd']);
fprintf('fliplr -> [%s;%s]  (expect ba;dc)\n', cm(1,:), cm(2,:));
cu = flipud(['ab';'cd']);
fprintf('flipud -> [%s;%s]  (expect cd;ab)\n', cu(1,:), cu(2,:));

fprintf('\n=== logical / complex / single ===\n');
fprintf('fliplr logical [1 1 0] -> %s  (expect [0 1 1])\n', mat2str(double(fliplr(logical([1 1 0])))));
cx = fliplr([1+1i 2+2i 3+3i]);
fprintf('fliplr complex re -> %s  (expect [3 2 1])\n', mat2str(real(cx)));
sg = flipud(single([1;2;3]));
fprintf('flipud single -> %s (class %s)  (expect [3;2;1], single)\n', mat2str(double(sg)), class(sg));

fprintf('\n=== flip(dim) char + cell still OK + double unchanged ===\n');
fd = flip(['ab';'cd'], 2);
fprintf('flip(.,2) char row1 = [%s]  (expect ba)\n', fd(1,:));
cc = fliplr({1,2,3});
fprintf('fliplr cell = {%g %g %g}  (expect 3 2 1)\n', cc{1}, cc{2}, cc{3});
disp(fliplr([1 2 3]));   % expect [3 2 1]
