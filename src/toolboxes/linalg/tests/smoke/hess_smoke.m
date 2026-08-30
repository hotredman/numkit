clear

A = [4 1 2; 1 3 7; 2 8 5];
[P, H] = hess(A);
fprintf('hess([4 1 2; 1 3 7; 2 8 5]):\n  H=\n'); disp(H);
fprintf('  H is upper-Hessenberg: H(3,1) = %g (must be 0)\n', H(3,1));
fprintf('  P*H*P'' - A: %g\n', max(max(abs(P*H*P' - A))));
fprintf('  P''*P - I: %g\n', max(max(abs(P'*P - eye(3)))));

B = [1 2 3 4; 5 6 7 8; 9 10 11 13; 1 0 1 2];
[Pb, Hb] = hess(B);
fprintf('\nhess 4x4 sub-sub-diagonal entries (must all be 0):\n');
fprintf('  H(3,1)=%g H(4,1)=%g H(4,2)=%g\n', Hb(3,1), Hb(4,1), Hb(4,2));
fprintf('  P*H*P'' - A: %g\n', max(max(abs(Pb*Hb*Pb' - B))));
