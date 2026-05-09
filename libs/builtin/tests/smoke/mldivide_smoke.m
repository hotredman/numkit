clear

import compat.*

% mldivide / mrdivide — matrix left/right division.
%   A\B  ↔ solve A·X = B
%   A/B  ↔ solve X·B = A   (equivalent to (B'\A')')
%
% Implemented via:
%   - LU with partial pivoting for square A
%   - QR via Householder for tall A (least squares)
%   - Wide A (m<n, min-norm) — DEFERRED, throws clear error
%
% All "expect" lines verified vs MATLAB R2025b.

A = [1 2; 3 4];
B = [5 6; 7 8];

fprintf('=== square 2x2: mrdivide(A, B)  →  X · B = A ===\n');
X = A / B;
fprintf('  X = '); disp(X);
fprintf('  expect [3 -2; 2 -1]\n\n');

fprintf('=== square 2x2: mldivide(A, B)  →  A · X = B ===\n');
Y = A \ B;
fprintf('  Y = '); disp(Y);
fprintf('  expect [-3 -4; 4 5]\n\n');

fprintf('=== identity ===\n');
fprintf('  A * (A\\B) - B  →  '); disp(A * (A\B) - B);
fprintf('  expect ~ zeros(2,2)\n\n');

fprintf('=== least squares (tall 4x2) ===\n');
At = [1 0; 1 1; 1 2; 1 3];
bt = [6; 5; 7; 10];
% Best-fit line  y = 4.9 + 1.4·x  per MATLAB
xls = At \ bt;
fprintf('  xls = '); disp(xls');
fprintf('  expect [4.9 1.4]\n');
