clear
import compat.*

% poly2mask — polygon scan-conversion to binary mask.
% Bit-equal MATLAB R2025b via half-open ray-cast rule.

fprintf('=== integer-aligned square [1,3] x [1,3] in 5x5 ===\n');
M1 = poly2mask([1 3 3 1], [1 1 3 3], 5, 5);
fprintf('sum = %d (expect 4)\n', sum(M1(:)));
disp(M1);

fprintf('=== triangle (1,1)-(4,1)-(1,4) ===\n');
M3 = poly2mask([1 4 1], [1 1 4], 5, 5);
fprintf('sum = %d (expect 3)\n', sum(M3(:)));
disp(M3);

fprintf('=== pentagon (irrational vertices) ===\n');
xp = 5+[2*cos((0:4)*2*pi/5)];
yp = 5+[2*sin((0:4)*2*pi/5)];
M7 = poly2mask(xp, yp, 11, 11);
fprintf('sum = %d (expect 10)\n', sum(M7(:)));

fprintf('\n=== self-intersecting bowtie ===\n');
M8 = poly2mask([1 5 3 5 1], [1 1 3 5 5], 5, 5);
fprintf('sum = %d (expect 12)\n', sum(M8(:)));

fprintf('\n=== large rectangle ===\n');
M9 = poly2mask([10 100 100 10], [10 10 80 80], 100, 110);
fprintf('sum = %d (expect 6300)\n', sum(M9(:)));

fprintf('\n=== regionfill via (I, X, Y) polygon form ===\n');
I = double(reshape(1:25, 5, 5));
J = regionfill(I, [2 4 4 2], [2 2 4 4]);
fprintf('J(3,3) = %g (expect 13)\n', J(3,3));
