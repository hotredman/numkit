clear

import compat.*

% axes2pix — convert world axis coords to pixel coords (1-based).

fprintf('--- MATLAB doc examples ---\n');
disp(axes2pix(800, [100 140], [100 120 140]));
fprintf('  expect: 1.0  400.5  800.0\n\n');

disp(axes2pix(600, [0 30], [0 15 30]));
fprintf('  expect: 1.0  300.5  600.0\n\n');

fprintf('--- reversed axis ---\n');
disp(axes2pix(800, [140 100], [100 120 140]));
fprintf('  expect: 800.0  400.5  1.0\n\n');

fprintf('--- Octave reference vectors ---\n');
fprintf('axes2pix(240, [1 240], 30) = %.2f (expect 30)\n', axes2pix(240, [1 240], 30));
fprintf('axes2pix(240, [400.5 520], 450) = %.2f (expect 100)\n', ...
        axes2pix(240, [400.5 520], 450));

fprintf('\n--- degenerate (n=1, extent=[1 1]) ---\n');
disp(axes2pix(1, [1 1], [0 1 2 3 4]));
fprintf('  expect: 0 1 2 3 4 (translation only)\n');
