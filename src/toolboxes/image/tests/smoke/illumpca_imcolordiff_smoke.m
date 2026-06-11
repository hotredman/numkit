clear;
import compat.*;

% Same deterministic 10×10×3 RGB image used in cycle 26's illum_smoke.
A = zeros(10, 10, 3);
for i = 1:10
  for j = 1:10
    A(i,j,1) = 0.01 * (10*(i-1) + (j-1));
    A(i,j,2) = 0.01 * (10*(j-1) + (i-1));
    A(i,j,3) = 1.0 - A(i,j,1);
  end
end

fprintf('--- illumpca (PCA-based illuminant) ---\n');
fprintf('  P=3.5  (default):        '); disp(illumpca(A));
fprintf('  P=1    (extremes only):  '); disp(illumpca(A, 1));
fprintf('  P=10:                    '); disp(illumpca(A, 10));
fprintf('  P=50   (use ALL pixels): '); disp(illumpca(A, 50));

fprintf('\n--- illumpca with bottom-5 mask ---\n');
M = true(10, 10); M(1:5, :) = false;
fprintf('  '); disp(illumpca(A, 3.5, 'Mask', M));

% imcolordiff CIE94 + CIEDE2000.
fprintf('\n--- imcolordiff (RGB inputs) ---\n');
I1 = [0.5 0.5 0.5; 0.8 0.2 0.3; 0.1 0.9 0.4];
I2 = [0.5 0.5 0.5; 0.7 0.3 0.4; 0.2 0.8 0.5];
fprintf('  CIE94  (default):\n');     disp(imcolordiff(I1, I2));
fprintf('  CIEDE2000:\n');             disp(imcolordiff(I1, I2, 'Standard', 'CIEDE2000'));

fprintf('--- imcolordiff (Lab inputs) ---\n');
L1 = [50 0 0; 60 5 -5; 70 -10 20];
L2 = [55 1 -1; 62 4 -6; 68 -12 22];
fprintf('  CIE94, isInputLab=true:\n'); disp(imcolordiff(L1, L2, 'isInputLab', true));
fprintf('  CIEDE2000, isInputLab=true:\n');
disp(imcolordiff(L1, L2, 'isInputLab', true, 'Standard', 'CIEDE2000'));

fprintf('--- imcolordiff with custom textile weighting (K1=0.048, K2=0.014) ---\n');
disp(imcolordiff(L1, L2, 'isInputLab', true, 'K1', 0.048, 'K2', 0.014));

% Image-shape input.
fprintf('--- imcolordiff image input ---\n');
I1img = reshape(I1, 1, 3, 3);  I2img = reshape(I2, 1, 3, 3);
v = imcolordiff(I1img, I2img);
fprintf('  size = [%d %d]   values: ', size(v,1), size(v,2)); disp(v);
