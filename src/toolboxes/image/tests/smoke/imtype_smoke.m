clear

% im2double: uint8 → [0, 1] double
fprintf('--- im2double(uint8([0 128 255])) ---\n');
disp(im2double(uint8([0 128 255])));
fprintf('  expect: [0 0.5020 1.0]\n\n');

% im2uint8: float → uint8 with saturation
fprintf('--- im2uint8([0 0.5 1.0 1.5 -0.1]) ---\n');
disp(double(im2uint8([0 0.5 1.0 1.5 -0.1])));
fprintf('  expect: [0 128 255 255 0]\n\n');

% im2uint16
fprintf('--- im2uint16([0 0.5 1.0]) ---\n');
disp(double(im2uint16([0 0.5 1.0])));
fprintf('  expect: [0 32768 65535]  (MATLAB rounds 32767.5→32768)\n\n');

% im2int16
fprintf('--- im2int16([0 0.5 1.0]) ---\n');
disp(double(im2int16([0 0.5 1.0])));
fprintf('  expect: [-32768 0 32767]\n\n');

% Round-trip
fprintf('--- im2double(im2uint8([0.2 0.5 0.8])) ---\n');
disp(im2double(im2uint8([0.2 0.5 0.8])));
fprintf('  expect: ≈ [0.2 0.5 0.8] (within uint8 quantisation)\n\n');

% mat2gray auto-detect
fprintf('--- mat2gray([10 20 30 40 50]) ---\n');
disp(mat2gray([10 20 30 40 50]));
fprintf('  expect: [0 0.25 0.5 0.75 1]\n\n');

% mat2gray with explicit range
fprintf('--- mat2gray([0 25 50 75 100], [25 75]) ---\n');
disp(mat2gray([0 25 50 75 100], [25 75]));
fprintf('  expect: [0 0 0.5 1 1]\n\n');

% rgb2gray
R = [100 150; 200 50];
G = [50 100; 150 200];
B = [25 75;  100 125];
RGB = cat(3, R, G, B);
fprintf('--- rgb2gray (Rec.601) ---\n');
disp(rgb2gray(RGB));
% Y = 0.2989*R + 0.5870*G + 0.1140*B
% (1,1) = 29.89 + 29.35 + 2.85 = 62.09 → 62
% (1,2) = 44.84 + 58.70 + 8.55 = 112.09 → 112
% (2,1) = 59.78 + 88.05 + 11.40 = 159.23 → 159
% (2,2) = 14.95 + 117.40 + 14.25 = 146.60 → 147
fprintf('  expect: [62 112; 159 147]\n\n');

% im2gray on grayscale = pass through
fprintf('--- im2gray of 2-D = pass-through ---\n');
disp(im2gray([1 2; 3 4]));
fprintf('  expect: [1 2; 3 4]\n\n');

% im2gray on RGB = rgb2gray
fprintf('--- im2gray of RGB ---\n');
disp(im2gray(RGB));
fprintf('  expect: same as rgb2gray\n');
