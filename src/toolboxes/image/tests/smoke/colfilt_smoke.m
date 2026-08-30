clear;

A = magic(5);
fprintf('Input A = magic(5):\n');
disp(A);

fprintf('\n--- (1) sliding 3x3 mean ---\n');
disp(colfilt(A, [3 3], 'sliding', @mean));

fprintf('--- (2) sliding 3x3 sum ---\n');
disp(colfilt(A, [3 3], 'sliding', @sum));

fprintf('--- (3) sliding 2x3 sum (even neighbourhood) ---\n');
disp(colfilt(A, [2 3], 'sliding', @sum));

fprintf('--- (4) indexed sliding 3x3 min (padval=1 for double) ---\n');
disp(colfilt(A, 'indexed', [3 3], 'sliding', @min));

fprintf('--- (5) distinct 2x2 shape-preserving (x.^2) ---\n');
B = colfilt(magic(6), [2 2], 'distinct', @(x) x.^2);
disp(B);

fprintf('--- (6) nlfilter ↔ colfilt equivalence (sum/3x3) ---\n');
Bn = nlfilter(A, [3 3], @(x) sum(x(:)));
Bc = colfilt(A, [3 3], 'sliding', @sum);
fprintf('  max |diff| = %g  (expect 0)\n', max(max(abs(Bn - Bc))));
