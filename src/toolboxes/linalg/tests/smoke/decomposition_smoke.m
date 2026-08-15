% toolboxes/linalg/tests/smoke/decomposition_smoke.m
%
% Smoke demo for decomposition object.

disp('--- decomposition object Smoke ---');

A = [1 2; 3 4];
b = [5; 6];

d = decomposition(A, 'lu');
x = solve(d, b);

disp('Solution x using cached LU decomposition (expected [-4; 4.5]):');
disp(x);
