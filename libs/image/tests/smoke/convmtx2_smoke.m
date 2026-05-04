clear

import compat.*

% convmtx2 — 2-D convolution matrix.

h = [1 2; 3 4];
fprintf('--- T = convmtx2([1 2;3 4], 3, 3) ---\n');
T = convmtx2(h, 3, 3);
fprintf('size: %s  (expect [16 9])\n', mat2str(size(T)));
disp(T);

fprintf('\n--- equivalence check: T*vec(I) == vec(conv2(I, h, ''full'')) ---\n');
I = [1 4 7; 2 5 8; 3 6 9];
v = T * I(:);
C = conv2(I, h, 'full');
fprintf('max|T*vec(I) - vec(conv2)|  = %.3e\n', max(abs(v - C(:))));

fprintf('\n--- single-arg vec form: convmtx2(h, [3 3]) ---\n');
T2 = convmtx2(h, [3 3]);
fprintf('matches scalar form? %d\n', isequal(T2, T));
