clear
import compat.*
% fprintf returns the number of BYTES written (MATLAB's `count` output).
% DEEP-PROBE 2026-05-31: numkit previously returned nothing, so
% `k = fprintf('hello')` left k undefined. Counts pinned vs MATLAB R2025b.

k = fprintf('hello\n');
fprintf('k=%d  (expect 6: "hello" + newline)\n', k);

n = fprintf('%d %d %d', [1 2 3]);
fprintf('\nn=%d  (expect 5: "1 2 3")\n', n);

% Format string recycled over a vector counts every emitted byte.
r = fprintf('%d-', [10 20 30]);
fprintf('\nr=%d  (expect 9: "10-20-30-")\n', r);

q = fprintf('%5.2f\n', 3.14159);
fprintf('q=%d  (expect 6: " 3.14" + newline)\n', q);

% Empty format -> 0 bytes.
z = fprintf('');
fprintf('z=%d  (expect 0)\n', z);

% Writing to stderr (fid==2) still reports the count.
e = fprintf(2, 'err');
fprintf('\ne=%d  (expect 3)\n', e);

% A bare fprintf with no LHS prints text but sets no `ans`.
disp('--- bare call below prints "bye" and NO ans = ... line ---');
fprintf('bye\n')
