% toolboxes/linalg/tests/smoke/funm_parlett_smoke.m
%
% Smoke demo for funm (Schur-Parlett general matrix functions).

disp('--- funm Schur-Parlett Smoke ---');

J = [1 1; 0 1];
F = funm(J, 'sin');

disp('funm([1 1; 0 1], ''sin'') (expected [sin(1) cos(1); 0 sin(1)]):');
disp(F);
