clear

import compat.*

% imadjustn — N-D variant of imadjust. In our build it's an alias since
% imadjust already handles 2-D and 3-D arrays elementwise.

% --- 2-D (same as imadjust) ---
fprintf('--- imadjustn (2-D) ---\n');
I = uint8(reshape(0:99, [10 10]));
y2 = imadjustn(I);
disp(double(y2(1:3, 1:3)));
fprintf('  expect: same as imadjust(I), stretched to [0,255]\n\n');

% --- 3-D RGB volume ---
fprintf('--- imadjustn (3-D, per-channel via stretchlim) ---\n');
V = uint8(cat(3, [10 20; 30 40], [100 110; 120 130], [200 210; 220 230]));
y3 = imadjustn(V);
fprintf('size: %s\n', mat2str(size(y3)));
disp(double(y3(:,:,1)));
fprintf('  expect: same shape as V, stretched\n\n');

% --- explicit endpoints ---
fprintf('--- imadjustn(I, [0.2 0.8]) ---\n');
J = double(0:0.1:1);
disp(imadjustn(J, [0.2 0.8]));
fprintf('  expect: [0..1] linearly remapped, clamped at endpoints\n');
