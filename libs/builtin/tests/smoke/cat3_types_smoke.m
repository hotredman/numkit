clear

import compat.*

% cat along dim 3 on non-DOUBLE arrays — DEEP-PROBE 2026-05-31. cat dim 1/2
% (vertcat/horzcat) were already type-agnostic, but the catDim3 page-concat
% path was DOUBLE-only and threw "Not a double array" on char/logical/complex/
% single/cell. cat is a pure rearrangement -> byte/cellAt page-concat (cell ->
% cell3D). Same-type only; mixed-type promotion + STRING deferred. Reference:
% MATLAB R2025b.

fprintf('=== char ===\n');
cm = cat(3, ['ab';'cd'], ['ef';'gh']);
fprintf('cat3 char size %s (1,1,1)=%s (1,1,2)=%s (2,2,2)=%s  (expect 2x2x2, a, e, h)\n', ...
        mat2str(size(cm)), cm(1,1,1), cm(1,1,2), cm(2,2,2));

fprintf('\n=== logical / single / complex / cell ===\n');
lg = cat(3, logical([1 0]), logical([0 1]));
fprintf('logical (1,1,1)=%g (1,2,2)=%g  (expect 1, 1)\n', double(lg(1,1,1)), double(lg(1,2,2)));
sg = cat(3, single([1 2]), single([3 4]));
fprintf('single (1,2,2)=%g class=%s  (expect 4, single)\n', sg(1,2,2), class(sg));
zc = cat(3, [1+1i 2+2i], [3+3i 4+4i]);
fprintf('complex (1,1,2)=%g+%gi  (expect 3+3i)\n', real(zc(1,1,2)), imag(zc(1,1,2)));
c3 = cat(3, {1,2}, {3,4});
fprintf('cell size %s c(1,1,2)=%g  (expect 1x2x2, 3)\n', mat2str(size(c3)), c3{1,1,2});

fprintf('\n=== multipage + empties + double unchanged ===\n');
mp = cat(3, cat(3, ['a';'b'], ['c';'d']), ['e';'f']);
fprintf('multipage size %s (1,1,3)=%s  (expect 2x1x3, e)\n', mat2str(size(mp)), mp(1,1,3));
ee = cat(3, ['ab';'cd'], [], ['ef';'gh']);
fprintf('with-empty size %s  (expect 2x2x2)\n', mat2str(size(ee)));
dd = cat(3, [1 2;3 4], [5 6;7 8]);
fprintf('double (1,1,2)=%g  (expect 5)\n', dd(1,1,2));
