% toolboxes/linalg/tests/smoke/ordschur_smoke.m
%
% Smoke demo for ordschur.

disp('--- ordschur Smoke ---');

T = [1 2; 0 3];
U = [1 0; 0 1];
[Us, Ts] = ordschur(U, T, [false, true]);

disp('Reordered T diagonal (expected 3, 1):');
disp(diag(Ts));

disp('Reconstruction residual Us*Ts*Us'' - T:');
disp(max(max(abs(Us*Ts*Us' - T))));
