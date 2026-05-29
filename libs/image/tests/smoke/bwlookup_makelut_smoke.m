clear
import compat.*

fprintf('=== makelut + bwlookup (neighbourhood lookup tables) ===\n');

BW = [0 1 1 0; 1 1 0 1; 0 0 1 1; 1 0 1 0];

% makelut on a 2x2 neighbourhood: 16-entry table.
l2 = makelut(@(x) sum(x(:)) >= 3, 2);
fprintf('makelut 2x2 (sum>=3): numel=%d sum=%d l2(8)=%d (expect 16, 5, 1)\n', ...
        numel(l2), sum(l2), l2(8));

% makelut on a 3x3 neighbourhood: 512-entry table.
lm = makelut(@(x) sum(x(:)) >= 5, 3);
fprintf('makelut 3x3 (sum>=5): numel=%d sum=%d (expect 512, 256)\n', numel(lm), sum(lm));

% Center-passthrough table -> bwlookup reproduces the input exactly.
lc = makelut(@(x) x(5), 3);
A = bwlookup(BW, lc);
fprintf('center lut -> bwlookup==BW? %d\n', isequal(logical(A), logical(BW)));

% bwlookup is the modern applylut (same index convention).
lp = double(mod(0:511, 2));
fprintf('bwlookup==applylut (parity 512): %d\n', isequal(bwlookup(BW, lp), applylut(BW, lp)));

% Output class follows the lut class.
A8 = bwlookup(BW, uint8(mod(0:511, 2)));
fprintf('bwlookup uint8 lut -> class=%s (expect uint8)\n', class(A8));

% A majority filter built from makelut.
maj = bwlookup(BW, lm);
fprintf('majority bwlookup: nnz=%d\n', nnz(maj));

fprintf('\n=== validation ===\n');
try; makelut(@(x) 1, 4); catch e; fprintf('makelut n=4: %s\n', strtok(e.message, char(10))); end
try; bwlookup(BW, [1 2 3]); catch e; fprintf('bwlookup lut3: %s\n', strtok(e.message, char(10))); end
