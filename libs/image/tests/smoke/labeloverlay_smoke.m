clear
import compat.*

% labeloverlay — overlay label / mask / colormap on a 2-D image.
% MATLAB reference values were captured in tmp/lo_probe.m and lo_probe3.m.

A = uint8([10 20 30 40 50; 60 70 80 90 100; 110 120 130 140 150; 160 170 180 190 200; 210 220 230 240 250]);
L = uint8([0 0 1 1 2; 0 1 1 2 2; 1 1 0 2 3; 1 0 0 3 3; 0 0 3 3 3]);

fprintf('=== Default: B = labeloverlay(A, L) ===\n');
B = labeloverlay(A, L);
fprintf('class(B) = %s, size = [%d %d %d]\n', class(B), size(B,1), size(B,2), size(B,3));
fprintf('B(1,1,:) = [%d %d %d] (expect [10 10 10])\n', B(1,1,1), B(1,1,2), B(1,1,3));
fprintf('B(1,3,:) = [%d %d %d] (expect [143 143 15])\n', B(1,3,1), B(1,3,2), B(1,3,3));
fprintf('B(1,5,:) = [%d %d %d] (expect [25 25 153])\n', B(1,5,1), B(1,5,2), B(1,5,3));
fprintf('B(3,5,:) = [%d %d %d] (expect [75 203 203])\n', B(3,5,1), B(3,5,2), B(3,5,3));

fprintf('\n=== Transparency = 0 (pure colour) ===\n');
B = labeloverlay(A, L, 'Transparency', 0);
fprintf('B(1,3,:) = [%d %d %d] (expect [255 255 0])\n', B(1,3,1), B(1,3,2), B(1,3,3));
fprintf('B(1,5,:) = [%d %d %d] (expect [0 0 255])\n', B(1,5,1), B(1,5,2), B(1,5,3));

fprintf('\n=== Transparency = 1 (pass-through) ===\n');
B = labeloverlay(A, L, 'Transparency', 1);
fprintf('B(1,3,:) = [%d %d %d] (expect [30 30 30])\n', B(1,3,1), B(1,3,2), B(1,3,3));

fprintf('\n=== Logical mask ===\n');
BW = logical([0 0 1 1 1; 0 1 1 1 0; 1 1 0 0 1; 0 0 1 1 1; 1 1 1 0 0]);
B = labeloverlay(A, BW);
fprintf('B(1,3,:) = [%d %d %d] (expect [15 15 143])\n', B(1,3,1), B(1,3,2), B(1,3,3));
fprintf('B(1,1,:) = [%d %d %d] (expect [10 10 10])\n', B(1,1,1), B(1,1,2), B(1,1,3));

fprintf('\n=== Custom Nx3 cmap, Transparency=0 ===\n');
cmap = [1 0 0; 0 1 0; 0 0 1];
B = labeloverlay(A, L, 'Colormap', cmap, 'Transparency', 0);
fprintf('B(1,3,:) = [%d %d %d] (expect [255 0 0]) label=1\n', B(1,3,1), B(1,3,2), B(1,3,3));
fprintf('B(1,5,:) = [%d %d %d] (expect [0 255 0]) label=2\n', B(1,5,1), B(1,5,2), B(1,5,3));
fprintf('B(3,5,:) = [%d %d %d] (expect [0 0 255]) label=3\n', B(3,5,1), B(3,5,2), B(3,5,3));

fprintf('\n=== IncludedLabels = [1 3] (skip label 2) ===\n');
B = labeloverlay(A, L, 'IncludedLabels', [1 3]);
fprintf('B(1,3,:) = [%d %d %d] (expect [143 143 15]) label=1 included\n', B(1,3,1), B(1,3,2), B(1,3,3));
fprintf('B(1,5,:) = [%d %d %d] (expect [50 50 50]) label=2 excluded\n', B(1,5,1), B(1,5,2), B(1,5,3));

fprintf('\n=== RGB input ===\n');
Arg = uint8(cat(3, [10 20 30; 40 50 60; 70 80 90], [100 110 120; 130 140 150; 160 170 180], [200 205 210; 215 220 225; 230 235 240]));
Lrg = uint8([0 1 1; 0 1 0; 2 2 0]);
B = labeloverlay(Arg, Lrg);
fprintf('class(B) = %s, size = [%d %d %d]\n', class(B), size(B,1), size(B,2), size(B,3));
fprintf('B(1,1,:) = [%d %d %d] (expect [10 100 200]) label 0 passthrough\n', B(1,1,1), B(1,1,2), B(1,1,3));
fprintf('B(1,2,:) = [%d %d %d] (expect [138 183 103]) label 1\n', B(1,2,1), B(1,2,2), B(1,2,3));
fprintf('B(3,1,:) = [%d %d %d] (expect [35 80 243]) label 2\n', B(3,1,1), B(3,1,2), B(3,1,3));

fprintf('\n=== ColorAssignment=noshuffle on jet ===\n');
A0 = uint8(zeros(1,3));
L0 = uint8([1 2 3]);
B = labeloverlay(A0, L0, 'Transparency', 0, 'ColorAssignment', 'noshuffle');
fprintf('label 1 -> [%d %d %d] (expect [0 0 255]) jet(4)[1]\n', B(1,1,1), B(1,1,2), B(1,1,3));
fprintf('label 2 -> [%d %d %d] (expect [0 255 255]) jet(4)[2]\n', B(1,2,1), B(1,2,2), B(1,2,3));
fprintf('label 3 -> [%d %d %d] (expect [255 255 0]) jet(4)[3]\n', B(1,3,1), B(1,3,2), B(1,3,3));
