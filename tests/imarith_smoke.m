import compat.*

% imadd: saturating uint8 addition
a = uint8([200 100 50; 0 255 128]);
b = uint8([100 50 30; 50 0 200]);
fprintf('--- imadd uint8 (saturating) ---\n');
disp(double(imadd(a, b)));
fprintf('  expect: [255 150 80; 50 255 255]\n\n');

% imsubtract: saturating uint8
fprintf('--- imsubtract uint8 ---\n');
disp(double(imsubtract(a, b)));
fprintf('  expect: [100 50 20; 0 255 0]\n\n');

% immultiply: saturating uint8
fprintf('--- immultiply uint8 ---\n');
disp(double(immultiply(uint8([10 20 30]), uint8([20 30 40]))));
fprintf('  expect: [200 255 255]\n\n');

% imdivide
fprintf('--- imdivide double ---\n');
disp(imdivide([10 20 30], [2 4 5]));
fprintf('  expect: [5 5 6]\n\n');

% imabsdiff
fprintf('--- imabsdiff uint8 ---\n');
disp(double(imabsdiff(uint8([10 200]), uint8([50 100]))));
fprintf('  expect: [40 100]\n\n');

% imcomplement
fprintf('--- imcomplement uint8 ---\n');
disp(double(imcomplement(uint8([0 100 255]))));
fprintf('  expect: [255 155 0]\n\n');

fprintf('--- imcomplement double in [0,1] ---\n');
disp(imcomplement([0 0.3 1.0]));
fprintf('  expect: [1.0 0.7 0]\n\n');

% imlincomb: 0.5*A + 0.5*B
fprintf('--- imlincomb 0.5*A + 0.5*B (double output) ---\n');
disp(imlincomb(0.5, [10 20 30], 0.5, [4 6 8]));
fprintf('  expect: [7 13 19]\n\n');

% imapplymatrix: simple gain matrix
fprintf('--- imapplymatrix on RGB stack ---\n');
M = eye(3);
X = cat(3, [1 2; 3 4], [5 6; 7 8], [9 10; 11 12]);
Y = imapplymatrix(M, X);
fprintf('Y(:,:,1) =\n'); disp(Y(:,:,1));
fprintf('Y(:,:,2) =\n'); disp(Y(:,:,2));
fprintf('Y(:,:,3) =\n'); disp(Y(:,:,3));
fprintf('  expect: copy of X\n');
