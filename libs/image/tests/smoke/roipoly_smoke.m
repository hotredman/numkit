clear
import compat.*

% roipoly — programmatic polygon ROI mask.
% Reference values from MATLAB R2025b.

A = zeros(5, 5);
xi = [1 4 4 1];
yi = [1 1 4 4];

fprintf('=== 3-arg (A, xi, yi) ===\n');
BW = roipoly(A, xi, yi);
fprintf('sum = %d (expect 9)\n', sum(BW(:)));

fprintf('\n=== 4-arg (M, N, xi, yi) ===\n');
BW = roipoly(5, 5, xi, yi);
fprintf('sum = %d (expect 9)\n', sum(BW(:)));

fprintf('\n=== 5-arg world coords (x, y, A, xi, yi) ===\n');
BW = roipoly([0 10], [0 10], A, [2 8 8 2], [2 2 8 8]);
fprintf('sum = %d (expect 9)\n', sum(BW(:)));

fprintf('\n=== 6-arg (x, y, M, N, xi, yi) ===\n');
BW = roipoly([0 10], [0 10], 5, 5, [2 8 8 2], [2 2 8 8]);
fprintf('sum = %d (expect 9)\n', sum(BW(:)));

fprintf('\n=== 2-output auto-close ===\n');
[BW, xi2] = roipoly(A, xi, yi);
fprintf('length(xi2) = %d (expect 5)\n', length(xi2));
fprintf('xi2(end) = %g (expect 1)\n', xi2(end));

fprintf('\n=== 5-output ===\n');
[xo, yo, BW, xi3, yi3] = roipoly(A, xi, yi);
fprintf('xo = [%g %g] yo = [%g %g] (expect [1 5] [1 5])\n', xo(1), xo(2), yo(1), yo(2));
