clear

A = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11];

fprintf('=== bounds ===\n');
[lo, hi] = bounds(A);
fprintf('  basic:  lo='); disp(lo); fprintf('         hi='); disp(hi);
[lo, hi] = bounds(A, 'all');
fprintf('  all:   [%g, %g]\n', lo, hi);
[lo, hi] = bounds(A, [1 2]);
fprintf('  vecdim: [%g, %g]\n', lo, hi);

fprintf('\n=== mode ===\n');
fprintf('  vector:    %d\n', mode([1 2 2 3 3 3]));
fprintf('  A all:     %d\n', mode(A, 'all'));
fprintf('  A vecdim:  %d\n', mode(A, [1 2]));
