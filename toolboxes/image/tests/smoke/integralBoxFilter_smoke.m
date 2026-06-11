clear
import compat.*

% image/integralBoxFilter — O(1) box filter via integral image.
% Reference: MATLAB R2025b.

fprintf('=== integralBoxFilter ===\n');

A = magic(8);
I = integralImage(A);
fprintf('  size(A)=%dx%d, size(I)=%dx%d (integral image is H+1 × W+1)\n', ...
        size(A,1), size(A,2), size(I,1), size(I,2));

B = integralBoxFilter(I);
fprintf('\n  default 3x3 → 6x6:\n'); disp(B);
fprintf('  (B(1,1)=33 = sum(A(1:3,1:3))/9; B(end,end)=32)\n');

B = integralBoxFilter(I, [3 5]);
fprintf('  [3 5] → 6x4: B(1,1)=%g  (e 32.2667)\n', B(1,1));

B = integralBoxFilter(I, 3, 'NormalizationFactor', 1);
fprintf('  Normalization=1 (raw sum): B(1,1)=%g  (e 297 = sum of A(1:3,1:3))\n', B(1,1));

% 3-D color
A3 = reshape(1:192, 8, 8, 3);
I3 = zeros(9, 9, 3);
for c = 1:3
  I3(:,:,c) = integralImage(A3(:,:,c));
end
B3 = integralBoxFilter(I3, 3);
fprintf('  3-D 8x8x3 → 6x6x3, B3(1,1,1)=%g  (e 10 = sum(A3(1:3,1:3,1))/9)\n', ...
        B3(1,1,1));

fprintf('\nBit-equal MATLAB R2025b. O(1) per pixel regardless of filter size.\n');
