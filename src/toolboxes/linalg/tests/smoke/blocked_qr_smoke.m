% toolboxes/linalg/tests/smoke/blocked_qr_smoke.m
%
% Smoke demo for blocked QR decomposition.

disp('--- blocked QR decomposition Smoke ---');

A = rand(256, 128);
[Q, R] = qr(A);

err = max(max(abs(A - Q * R)));
disp('Reconstruction error ||A - Q*R|| (expected < 1e-12):');
disp(err);
