clear

fprintf('=== norm (vector forms) ===\n');
v = [3 4];
fprintf('  norm([3 4]) = %g (expect 5)\n', norm(v));
fprintf('  norm([3 4], 1) = %g (expect 7)\n', norm(v, 1));
fprintf('  norm([3 4], Inf) = %g (expect 4)\n', norm(v, Inf));
fprintf('  norm([3 4], -Inf) = %g (expect 3 = min|v|)\n', norm(v, -Inf));
fprintf('  norm([-7 2 5], -Inf) = %g (expect 2)\n', norm([-7 2 5], -Inf));

vp = [1 -2 3 -4 5];
fprintf('  norm(v, 4) = %g\n', norm(vp, 4));

fprintf('\n=== norm (matrix forms) ===\n');
A = [1 2; 3 4];
fprintf('  norm(A) = %g (= largest sigma)\n', norm(A));
fprintf('  norm(A, 1) = %g (= max col sum)\n', norm(A, 1));
fprintf('  norm(A, Inf) = %g (= max row sum)\n', norm(A, Inf));
fprintf('  norm(A, ''fro'') = %g (= sqrt(sum(A.^2)))\n', norm(A, 'fro'));
