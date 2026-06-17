clear

import compat.*

% permute / ipermute on non-DOUBLE arrays — DEEP-PROBE 2026-05-31. The
% DOUBLE-only path threw "Not a double array" on char/logical/complex/single/
% cell. permute is a pure rearrangement, so it is type-preserving (strided
% byte/cellAt gather). Reference: MATLAB R2025b.

fprintf('=== char transpose ===\n');
cm = permute(['ab';'cd'], [2 1]);
fprintf('permute([2 1]) -> [%s;%s]  (expect ac;bd)\n', cm(1,:), cm(2,:));

fprintf('\n=== cell ===\n');
cp = permute({1,2;3,4}, [2 1]);
fprintf('cell(2,1)=%g cell(1,2)=%g  (expect 2, 3)\n', cp{2,1}, cp{1,2});

fprintf('\n=== logical / complex / single ===\n');
fprintf('logical: %s  (expect [1 0;0 1])\n', mat2str(double(permute(logical([1 0;0 1]), [2 1]))));
zx = permute([1+1i 2; 3 4], [2 1]);
fprintf('complex (1,2) = %g+%gi  (expect 3+0i)\n', real(zx(1,2)), imag(zx(1,2)));
sg = permute(single([1 2;3 4]), [2 1]);
fprintf('single -> %s (class %s)  (expect [1 3;2 4], single)\n', mat2str(double(sg)), class(sg));

fprintf('\n=== 3-D char + ipermute round-trip + double unchanged ===\n');
p3 = permute(reshape('abcdefgh',2,2,2), [2 1 3]);
fprintf('3d char p3(1,2,1) = %s  (expect b)\n', p3(1,2,1));
rt = ipermute(['ac';'bd'], [2 1]);
fprintf('ipermute -> [%s;%s]  (expect ab;cd)\n', rt(1,:), rt(2,:));
disp(permute([1 2;3 4], [2 1]));   % expect [1 3;2 4]
