clear

import compat.*

% entropy — Shannon entropy in bits.

fprintf('--- Octave-source test vectors ---\n');
fprintf('entropy([0 1])         = %.6f (expect 1)\n',     entropy([0 1]));
fprintf('entropy(uint8([0 1]))  = %.6f (expect 1)\n',     entropy(uint8([0 1])));
fprintf('entropy([0 0])         = %.6f (expect 0)\n',     entropy([0 0]));
fprintf('entropy([0])           = %.6f (expect 0)\n',     entropy([0]));
fprintf('entropy([1])           = %.6f (expect 0)\n',     entropy([1]));
fprintf('entropy([0 .5; 2 0])   = %.6f (expect 1.5)\n',   entropy([0 0.5; 2 0]));

fprintf('\n--- larger image ---\n');
A = uint8(reshape(0:255, [16 16]));
fprintf('entropy(uint8 0..255)  = %.4f (expect 8.0 — uniform 256 bins)\n', ...
        entropy(A));

fprintf('\n--- explicit nbins ---\n');
fprintf('entropy(A, 16) = %.4f (expect 4.0 — 16 uniform bins)\n', ...
        entropy(A, 16));
