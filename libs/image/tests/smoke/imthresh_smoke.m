import compat.*

% Bimodal image: half dark, half bright
I = uint8([10 20 30 200 210 220; 15 25 35 205 215 225]);

% graythresh — Otsu should pick something between the modes
fprintf('--- graythresh ---\n');
[t, em] = graythresh(I);
fprintf('thresh = %.4f, em = %.4f\n', t, em);
fprintf('  expect: thresh ≈ 0.5 (between dark ~30 and bright ~210), em > 0.9\n\n');

% otsuthresh from precomputed histogram
fprintf('--- otsuthresh(imhist(I)) ---\n');
[c, ~] = imhist(I);
[t2, em2] = otsuthresh(c);
fprintf('thresh = %.4f, em = %.4f\n', t2, em2);
fprintf('  expect: same as graythresh\n\n');

% multithresh with N=2
fprintf('--- multithresh(I, 2) ---\n');
[t3, em3] = multithresh(I, 2);
disp(t3); fprintf('em = %.4f\n', em3);
fprintf('  expect: 2 thresholds dividing into 3 levels\n\n');

% imbinarize with default threshold
fprintf('--- imbinarize(I) [auto threshold] ---\n');
disp(double(imbinarize(I)));
fprintf('  expect: dark→0, bright→1\n\n');

% imbinarize with explicit threshold
fprintf('--- imbinarize(I, 0.5) ---\n');
disp(double(imbinarize(I, 0.5)));
fprintf('  expect: same — dark→0, bright→1\n\n');

% imquantize
fprintf('--- imquantize([0.1 0.3 0.5 0.7 0.9], [0.4 0.8]) ---\n');
disp(imquantize([0.1 0.3 0.5 0.7 0.9], [0.4 0.8]));
fprintf('  expect: [1 1 2 2 3] (3-class quantize at 0.4 and 0.8)\n');
