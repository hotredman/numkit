clear

import compat.*

% rgb2lin — sRGB → linear RGB.

fprintf('--- pure 0.5 (above d=0.04045 → gamma branch) ---\n');
v = rgb2lin(0.5);
fprintf('rgb2lin(0.5) = %.10f\n', v);
fprintf('  expect ((0.5+0.055)/1.055)^2.4 = %.10f\n', ...
        ((0.5+0.055)/1.055)^2.4);

fprintf('\n--- below threshold (linear branch) ---\n');
v = rgb2lin(0.03);
fprintf('rgb2lin(0.03) = %.10f\n', v);
fprintf('  expect 0.03/12.92 = %.10f\n', 0.03/12.92);

fprintf('\n--- 0 and 1 fixed points ---\n');
fprintf('rgb2lin(0) = %.10f  (expect 0)\n', rgb2lin(0));
fprintf('rgb2lin(1) = %.10f  (expect 1)\n', rgb2lin(1));

fprintf('\n--- negative input → mirrored ---\n');
fprintf('rgb2lin(-0.5) = %.10f  (expect -((0.5+0.055)/1.055)^2.4)\n', rgb2lin(-0.5));

fprintf('\n--- H×W×3 image ---\n');
rng('default');
A = rand(3, 3, 3);
B = rgb2lin(A);
fprintf('size: %s\n', mat2str(size(B)));
fprintf('B(1,1,:) = [%.6f %.6f %.6f]\n', B(1,1,1), B(1,1,2), B(1,1,3));
