clear

import compat.*

% mov* family — nanflag + Endpoints support (audit ТЗ closures).
A  = [1 3 2 5 4 6 NaN 8 7 10]';
A2 = (1:9)';

fprintf('=== nanflag default (includenan; NaN poisons) ===\n');
disp(movmean(A, 3)');
fprintf('  expect: 2 2 3.333 3.667 5 NaN NaN NaN 8.333 8.5\n\n');

fprintf('=== nanflag explicit omitnan ===\n');
disp(movmean(A, 3, 'omitnan')');
fprintf('  expect: 2 2 3.333 3.667 5 5 7 7.5 8.333 8.5\n\n');

fprintf('=== nanflag omitmissing alias ===\n');
y1 = movmean(A, 3, 'omitnan');
y2 = movmean(A, 3, 'omitmissing');
fprintf('  max diff vs omitnan: %.4e (expect 0)\n\n', max(abs(y1 - y2)));

fprintf('=== Endpoints discard (output shorter) ===\n');
y = movmean(A2, 3, 'Endpoints', 'discard');
fprintf('  size = %d (expect 7)\n', length(y));
disp(y');
fprintf('  expect: 2 3 4 5 6 7 8\n\n');

fprintf('=== Endpoints fill (NaN at edges) ===\n');
disp(movmean(A2, 3, 'Endpoints', 'fill')');
fprintf('  expect: NaN 2 3 4 5 6 7 8 NaN\n\n');

fprintf('=== Endpoints scalar 0 (pad missing window slots with 0) ===\n');
disp(movmean(A2, 3, 'Endpoints', 0)');
fprintf('  expect: 1 2 3 4 5 6 7 8 5.667\n\n');

fprintf('=== matrix + dim + nanflag + Endpoints combined ===\n');
M = [A2 A2*2];
y = movmean(M, 3, 1, 'omitnan', 'Endpoints', 'discard');
fprintf('  size = %dx%d (expect 7x2)\n', size(y, 1), size(y, 2));
disp(y);
fprintf('  expect column 1: 2 3 4 5 6 7 8 ; column 2: 4 6 8 10 12 14 16\n\n');

fprintf('=== other family members with omitnan (matches MATLAB) ===\n');
fprintf('  movmedian: '); disp(movmedian(A, 3, 'omitnan')');
fprintf('  movstd:    '); disp(movstd(A, 3, 'omitnan')');
fprintf('  movmad:    '); disp(movmad(A, 3, 'omitnan')');
fprintf('  movvar(,1): '); disp(movvar(A, 3, 1, 'omitnan')');

fprintf('=== k=0 errors (MATLAB-matching message) ===\n');
try
    movmean(A2, 0);
catch err
    fprintf('  caught: %s\n', err.message);
end

% Even-length scalar window leans BACKWARD (current+previous), MATLAB R2025b.
fprintf('=== even-window backward alignment ===\n');
fprintf('  movsum([1 2 3 4],2)   = %s (expect [1 3 5 7])\n', mat2str(movsum([1 2 3 4],2)));
fprintf('  movmean([1 2 3 4],2)  = %s (expect [1 1.5 2.5 3.5])\n', mat2str(movmean([1 2 3 4],2)));
fprintf('  movmax([1 5 2 8],2)   = %s (expect [1 5 5 8])\n', mat2str(movmax([1 5 2 8],2)));
fprintf('  movsum([1 2 3 4 5 6],4) = %s (expect [3 6 10 14 18 15])\n', mat2str(movsum([1 2 3 4 5 6],4)));
fprintf('  movsum([1 2 3 4],3) = %s (odd unchanged, expect [3 6 9 7])\n', mat2str(movsum([1 2 3 4],3)));
