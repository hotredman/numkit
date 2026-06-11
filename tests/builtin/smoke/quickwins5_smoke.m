clear
import compat.*

fprintf('=== trace ===\n');
A = [1 2 3; 4 5 6; 7 8 9];
fprintf('  trace(magic-like) = %g (expect 15)\n', trace(A));
fprintf('  trace(eye(7)) = %g (expect 7)\n', trace(eye(7)));

fprintf('\n=== det ===\n');
fprintf('  det([4 7; 2 6]) = %g (expect 10)\n', det([4 7; 2 6]));
fprintf('  det(magic(4)) = %g (expect 0 -- magic(4) is singular)\n', det(magic(4)));
fprintf('  det(hilb(4)) = %g (expect ~1.65e-7)\n', det(hilb(4)));

fprintf('\n=== chol ===\n');
S = [4 12 -16; 12 37 -43; -16 -43 98];
R = chol(S);
disp(R);
fprintf('  R'' * R - S max error: %g (expect 0)\n', max(max(abs(R'*R - S))));

fprintf('\n=== topkrows ===\n');
M = [3 1; 1 5; 2 4; 5 2; 4 3];
T = topkrows(M, 3);
fprintf('  Top 3 of (5x2 matrix), lex-descending:\n');
disp(T);

fprintf('\n=== cputime ===\n');
t1 = cputime;
for k = 1:100000; x = sin(k); end
t2 = cputime;
fprintf('  cputime delta over 100k iterations: %g s\n', t2 - t1);
