clear;

% Deterministic 10×10×3 RGB image: R = ramp by row+col, G = ramp by
% col+row, B = 1 - R. All values exactly representable in binary.
A = zeros(10, 10, 3);
for i = 1:10
  for j = 1:10
    A(i,j,1) = 0.01 * (10*(i-1) + (j-1));
    A(i,j,2) = 0.01 * (10*(j-1) + (i-1));
    A(i,j,3) = 1.0 - A(i,j,1);
  end
end

fprintf('--- illumwhite ---\n');
fprintf('  P=0  (per-channel max):   '); disp(illumwhite(A, 0));
fprintf('  P=1  (default):           '); disp(illumwhite(A));
fprintf('  P=5  (top 5%%):            '); disp(illumwhite(A, 5));
fprintf('  P=50 (top half by chan):  '); disp(illumwhite(A, 50));

fprintf('\n--- illumgray ---\n');
fprintf('  P=1  (default both ends): '); disp(illumgray(A));
fprintf('  P=[10 10]:                '); disp(illumgray(A, [10 10]));
fprintf('  P=25 (scalar = both):     '); disp(illumgray(A, 25));
fprintf('  P=1, Norm=2 (RMS):        '); disp(illumgray(A, 1, 'Norm', 2));

fprintf('\n--- Mask exercise ---\n');
M = true(10, 10);
M(1:5, :) = false;       % use only bottom-5 rows
fprintf('  illumwhite with bottom-5 mask:  '); disp(illumwhite(A, 0, 'Mask', M));
fprintf('  illumgray  with bottom-5 mask:  '); disp(illumgray(A,  0, 'Mask', M));
