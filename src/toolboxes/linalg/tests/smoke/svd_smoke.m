clear

fprintf('=== svd singular values ===\n');
fprintf('  svd(diag([3 1 2])) = '); disp(svd(diag([3 1 2]))');

A = [1 2 3; 4 5 6; 7 8 10];
[U, S, V] = svd(A);
fprintf('\n=== svd 3x3 full ===\n');
fprintf('  s = [%g %g %g]\n', S(1,1), S(2,2), S(3,3));
fprintf('  U*S*V'' - A max err: %g\n', max(max(abs(U*S*V' - A))));
fprintf('  U orthogonal:        %g\n', max(max(abs(U'*U - eye(3)))));
fprintf('  V orthogonal:        %g\n', max(max(abs(V'*V - eye(3)))));

fprintf('\n=== svd 4x3 tall ===\n');
At = [1 2; 3 4; 5 6; 7 8];
[Ut, St, Vt] = svd(At);
fprintf('  s = [%g %g]\n', St(1,1), St(2,2));
fprintf('  U*S*V'' - A max err: %g\n', max(max(abs(Ut*St*Vt' - At))));

fprintf('\n=== svd rank-deficient ===\n');
Ar = [1 2; 2 4; 3 6];   % all rows multiples of [1 2]
s = svd(Ar);
fprintf('  s = [%g %g]\n', s(1), s(2));
fprintf('  s(2) ~ 0 (rank 1)\n');
