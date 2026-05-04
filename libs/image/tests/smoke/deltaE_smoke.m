clear

import compat.*

% deltaE — CIE76 colour difference (Euclidean in CIELAB).

% Use isInputLab=true to bypass the rgb2lab conversion (which has a
% known divergence vs MATLAB) and validate just the sqrt-sum kernel.

fprintf('--- Lab inputs (M-by-3 colormap) ---\n');
A = [50 0 0; 50 10 0; 50 0 10];
B = [50 0 0; 50  0 0; 50 0  0];
de = deltaE(A, B, 'isInputLab', true);
disp(de);
fprintf('  expect [0; 10; 10]\n\n');

fprintf('--- Lab inputs (H-by-W-by-3 image) ---\n');
A = zeros(2,2,3); A(:,:,1) = 50; A(:,:,2) = [0 5; 5 0];
B = zeros(2,2,3); B(:,:,1) = 50; B(:,:,2) = [0 0; 0 0];
de = deltaE(A, B, 'isInputLab', true);
disp(de);
fprintf('  expect [0 5; 5 0]\n');
