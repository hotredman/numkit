clear

import compat.*

% sgolayfilt matrix + weights + dim (DEEP-PROBE 2026-05-31). MATLAB's
% sgolayfilt filters each 1-D slice of a matrix along dim (default = first
% non-singleton: columns for a matrix), and supports a weighted
% least-squares fit. numkit previously errored on matrices ("input must be
% a vector") and silently ignored the weights/dim arguments. vs MATLAB R2025b.

fprintf('=== weighted vs unweighted (vector) ===\n');
x = [2 5 1 8 3 9 4 7 6];
yu = sgolayfilt(x, 2, 5);
yw = sgolayfilt(x, 2, 5, [1 2 3 2 1]);
fprintf('unweighted y(1)=%.5f y(9)=%.5f\n', yu(1), yu(9));
fprintf('weighted   y(1)=%.5f y(9)=%.5f  (expect 2.53333 5.86667)\n', yw(1), yw(9));
fprintf('weighting changed output: %d (expect 1)\n', abs(yw(1) - yu(1)) > 1e-6);

fprintf('\n=== matrix, per-column (default dim = 1) ===\n');
C = sgolayfilt([2 5; 1 8; 3 9; 4 7; 6 2], 1, 3);
fprintf('size(C) = [%d %d]  (expect [5 2])\n', size(C,1), size(C,2));
fprintf('C(:,1)'' = [%.4f %.4f %.4f %.4f %.4f]\n', C(1,1), C(2,1), C(3,1), C(4,1), C(5,1));
fprintf('         (expect [1.5000 2.0000 2.6667 4.3333 5.8333])\n');
fprintf('C(5,2) = %.4f  (expect 2.5000)\n', C(5,2));

fprintf('\n=== matrix, per-row (dim = 2) ===\n');
R = sgolayfilt([2 5 1 8 3; 9 4 7 6 2], 1, 3, [], 2);
fprintf('size(R) = [%d %d]  (expect [2 5])\n', size(R,1), size(R,2));
fprintf('R(1,:) = [%.4f %.4f %.4f %.4f %.4f]\n', R(1,1), R(1,2), R(1,3), R(1,4), R(1,5));
fprintf('       (expect [3.1667 2.6667 4.6667 4.0000 5.0000])\n');
fprintf('R(2,5) = %.4f  (expect 2.5000)\n', R(2,5));
