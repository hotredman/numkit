clear

% grayslice — multilevel thresholding into an indexed image.

% --- default n=10 on float ---
fprintf('--- grayslice([0 .45 .5 .55 1]) (default n=10) ---\n');
disp(double(grayslice([0 0.45 0.5 0.55 1])));
fprintf('  expect: [0 4 5 5 9]\n\n');

% --- explicit n=4 ---
fprintf('--- grayslice([0 .45 .5 .55 1], 4) ---\n');
disp(double(grayslice([0 0.45 0.5 0.55 1], 4)));
fprintf('  expect: [0 1 2 2 3]\n\n');

% --- explicit threshold vector ---
fprintf('--- grayslice([0 .45 .5 .55 1], [0 .5 1]) ---\n');
disp(double(grayslice([0 0.45 0.5 0.55 1], [0 0.5 1])));
fprintf('  expect: [1 1 2 2 3]\n\n');

% --- uint8 input with vector V (in image's value range) ---
fprintf('--- grayslice(uint8([0 100 200 255]), [100 199 200 210]) ---\n');
disp(double(grayslice(uint8([0 100 200 255]), [100 199 200 210])));
fprintf('  expect: [0 1 3 4]\n\n');

% --- 0 < n < 1 treated as a single-element V ---
fprintf('--- grayslice([0 .5 .55 .7 1], 0.5) ---\n');
disp(double(grayslice([0 0.5 0.55 0.7 1], 0.5)));
fprintf('  expect: [0 1 1 1 1]\n');
