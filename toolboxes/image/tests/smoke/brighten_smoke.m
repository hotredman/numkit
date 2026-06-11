clear

import compat.*

% brighten — gamma-adjust an N×3 colormap.

M = [0 0.25 0.5 0.75 1; 0 0.25 0.5 0.75 1; 0 0.25 0.5 0.75 1]';

fprintf('--- beta = 0.5 (brighter) ---\n');
disp(brighten(M, 0.5));
fprintf('  expect col1 = [0; 0.5; 0.7071; 0.8660; 1]  (sqrt of [0..1])\n\n');

fprintf('--- beta = -0.5 (darker) ---\n');
disp(brighten(M, -0.5));
fprintf('  expect col1 = [0; 0.0625; 0.25; 0.5625; 1]  (^2)\n\n');

fprintf('--- beta = 0 (identity) ---\n');
disp(brighten(M, 0));
fprintf('  expect identity\n');
