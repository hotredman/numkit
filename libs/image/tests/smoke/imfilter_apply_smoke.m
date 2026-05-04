clear

import compat.*

A = [1 2 3 4 5; 6 7 8 9 10; 11 12 13 14 15; 16 17 18 19 20; 21 22 23 24 25];

% imfilter with average kernel = local mean
fprintf('--- imfilter(A, fspecial(''average'', 3)) ---\n');
disp(imfilter(A, fspecial('average', 3)));
fprintf('  expect interior(2,2)..(4,4) = 7..19 (interior averages)\n\n');

% imfilter with sobel kernel = vertical-edge response
fprintf('--- imfilter(A, fspecial(''sobel''), ''replicate'') ---\n');
disp(imfilter(A, fspecial('sobel'), 'replicate'));
fprintf('  expect: -30 in interior (constant gradient 5/row × 6 = 30)\n\n');

% imboxfilt convenience
fprintf('--- imboxfilt(A, 3) ---\n');
disp(imboxfilt(A, 3));
fprintf('  expect: 3×3 mean filter w/ replicate boundary\n\n');

% imgaussfilt
fprintf('--- imgaussfilt(A, 1) ---\n');
disp(imgaussfilt(A, 1));
fprintf('  expect: smoothed A, interior values close to original\n\n');

% medfilt2 — salt-and-pepper test
B = [1 2 3 4; 5 99 7 8; 9 10 11 12; 13 14 15 16];
fprintf('--- medfilt2(B) — outlier 99 should disappear ---\n');
disp(medfilt2(B));
fprintf('  expect (2,2) = median of [1,2,3,5,99,7,9,10,11] = 7\n\n');

fprintf('--- medfilt2 with [5 5] kernel ---\n');
disp(medfilt2(B, [5 5]));
fprintf('  expect: more aggressive smoothing\n');
