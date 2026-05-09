clear
import compat.*

fprintf('=== toeplitz ===\n');
T = toeplitz([1 2 3 4]);
fprintf('  toeplitz([1 2 3 4]) -- expect 4x4 symmetric Toeplitz\n');
disp(T);
fprintf('  symmetric: %d (expect 1)\n\n', max(max(abs(T - T'))) == 0);

fprintf('=== hankel ===\n');
H = hankel([1 2 3 4]);
fprintf('  hankel([1 2 3 4]) -- expect anti-Toeplitz with trailing zeros\n');
disp(H);
fprintf('  H(2,3) = %g (expect 4)\n\n', H(2,3));

fprintf('=== vander ===\n');
V = vander([1 2 3 4]);
fprintf('  vander([1 2 3 4]) -- highest power on the LEFT\n');
disp(V);
fprintf('  last col all ones: %d (expect 1)\n\n', all(V(:,end) == 1));

fprintf('=== compan ===\n');
C = compan([1 0 -7 6]);   % roots = 1, 2, -3
fprintf('  compan([1 0 -7 6]) -- companion of x^3 - 7x + 6\n');
disp(C);
fprintf('  top row: [%g %g %g] (expect [0 7 -6])\n', C(1,1), C(1,2), C(1,3));
