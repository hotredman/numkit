clear

import compat.*

fprintf('=== kaiser ===\n');

% beta=0 -> rectangular
w0 = kaiser(8, 0);
fprintf('  kaiser(8, 0)    : '); fprintf('%.4f ', w0); fprintf('\n');
fprintf('  expect: all 1.0000 (rectangular)\n');

% beta=5 (Hamming-like)
w5 = kaiser(16, 5);
fprintf('  kaiser(16, 5)   : edge=%.4f center=%.4f (Hamming-like)\n', w5(1), w5(8));

% beta=8.6 (Blackman-like)
w86 = kaiser(64, 8.6);
fprintf('  kaiser(64, 8.6) : edge=%.6f center=%.4f (Blackman-like)\n', w86(1), w86(32));

% Default beta=0.5
wd = kaiser(8);
fprintf('  kaiser(8) (default β=0.5) edge=%.4f\n', wd(1));

% Length 1 -> always [1]
fprintf('  kaiser(1, 5)    : %.4f (single-point = 1)\n', kaiser(1, 5));
