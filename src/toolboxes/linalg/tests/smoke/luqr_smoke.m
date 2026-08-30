clear

fprintf('=== lu ===\n');
A = [1 2 3; 4 5 6; 7 8 10];
[L, U, P] = lu(A);
fprintf('  L=\n'); disp(L);
fprintf('  U=\n'); disp(U);
fprintf('  P=\n'); disp(P);
fprintf('  P*A - L*U max err: %g (expect 0)\n\n', max(max(abs(P*A - L*U))));

fprintf('=== qr (square) ===\n');
[Q, R] = qr(A);
fprintf('  Q*R - A max err: %g\n', max(max(abs(Q*R - A))));
fprintf('  Q is orthogonal (Q''*Q - I): %g\n', max(max(abs(Q'*Q - eye(3)))));
fprintf('  R lower triangle: max(R(2,1), R(3,1), R(3,2)) = %g\n\n', ...
        max([abs(R(2,1)) abs(R(3,1)) abs(R(3,2))]));

fprintf('=== qr (tall) ===\n');
At = [1 2; 3 4; 5 6];
[Qt, Rt] = qr(At);
fprintf('  size(Q)=[%d %d], size(R)=[%d %d]\n', size(Qt,1), size(Qt,2), size(Rt,1), size(Rt,2));
fprintf('  Q*R - A max err: %g\n', max(max(abs(Qt*Rt - At))));
