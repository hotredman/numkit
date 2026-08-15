% toolboxes/linalg/tests/smoke/qr_complex_smoke.m
%
% Smoke demo for complex QR decomposition and rectangular least-squares solve.

disp('--- Complex QR Smoke ---');
A = [1+1i, 2; 3, 4-1i; 0, 1+2i];
b = [1; 2+1i; 3-1i];

[Q, R] = qr(A);
disp('Q * R - A residual:');
disp(max(max(abs(Q*R - A))));

disp('Q''*Q - I residual:');
disp(max(max(abs(Q'*Q - eye(3)))));

x = A \ b;
disp('Least squares solution x = A \ b:');
disp(x);

disp('Normal equation residual A'' * (A*x - b):');
disp(A' * (A*x - b));
