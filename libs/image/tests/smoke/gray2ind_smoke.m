clear

import compat.*

% gray2ind / ind2gray — grayscale ↔ indexed image.

% --- Octave-source reference vectors ---
fprintf('--- gray2ind doc test ---\n');
fprintf('gray2ind([0 0.25 0.5 1])         = '); disp(double(gray2ind([0 0.25 0.5 1])));
fprintf('  expect: [0 16 32 63]\n\n');

fprintf('gray2ind([0 0.25 0.5 1], 400)    = '); disp(double(gray2ind([0 0.25 0.5 1], 400)));
fprintf('  expect: [0 100 200 399] (uint16)\n\n');

fprintf('gray2ind(logical([1 0 0 1]))     = '); disp(double(gray2ind(logical([1 0 0 1]))));
fprintf('  expect: [1 0 0 1]\n\n');

fprintf('gray2ind(uint8([0 64 128 192 255]))= '); disp(double(gray2ind(uint8([0 64 128 192 255]))));
fprintf('  expect: [0 16 32 47 63]\n\n');

% --- ind2gray ---
fprintf('--- ind2gray round-trip ---\n');
img = [0 0.25 0.5 0.75 1.0];
[ind, map] = gray2ind(img, 64);
back = ind2gray(double(ind) + 1, map);   % +1 because ind2gray with float idx is 1-based
fprintf('round-trip: max|err| = %.4e\n', max(abs(img(:) - back(:))));
