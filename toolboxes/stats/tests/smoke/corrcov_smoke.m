clear
import compat.*

fprintf('=== corrcov (correlation matrix from covariance) ===\n');

C = [4 2 1; 2 9 3; 1 3 16];
[R, sigma] = corrcov(C);
fprintf('  corrcov of [4 2 1; 2 9 3; 1 3 16]:\n');
disp(R)
fprintf('  expect:\n    1.0000    0.3333    0.1250\n    0.3333    1.0000    0.2500\n    0.1250    0.2500    1.0000\n');
fprintf('  sigma = '); fprintf('%g ', sigma); fprintf('  (expect 2 3 4)\n');

[R2, s2] = corrcov(eye(3));
fprintf('\n  corrcov(eye(3)):\n');
disp(R2)
fprintf('  sigma = '); fprintf('%g ', s2); fprintf('  (expect 1 1 1)\n');

[R3, s3] = corrcov(5);
fprintf('\n  corrcov(5) = %g, sigma = %g (expect 1, 2.23607)\n', R3, s3);

% Negative correlation
R4 = corrcov([4 -2; -2 1]);
fprintf('\n  corrcov([4 -2; -2 1]):\n');
disp(R4)
fprintf('  expect [1 -1; -1 1]\n');
