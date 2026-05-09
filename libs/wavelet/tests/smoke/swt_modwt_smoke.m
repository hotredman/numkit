clear

import compat.*

% SWT / MODWT batch smoke — audit ТЗ closure 2026-05-09.
%
% Real fixes this cycle:
%   - modwt argument order: (x, lev, wname) → (x, wname, lev) per
%     MATLAB R2025b. Plus default wname='sym4' and default lev =
%     floor(log2(N)). Pre-fix, modwt(x, 'haar', 3) THREW.
%
% Known kernel-level gaps (out of scope, separate audit):
%   - swt detail-row sign differs from MATLAB (Hi_D vs Hi_R QMF).
%     Approximation row matches.
%   - modwt per-coefficient values diverge from MATLAB (sqrt(2)
%     normalisation). Output shape matches.
%   - BOTH inverses (iswt, imodwt) recover the original signal to
%     machine precision — that's the structurally important invariant.

x = sin(2*pi*0.1*(0:31)') + 0.3*cos(2*pi*0.05*(0:31)');

fprintf('=== modwt — MATLAB argument order now accepted ===\n');
w1 = modwt(x, 'haar', 3);
fprintf('  modwt(x, ''haar'', 3) → size = %d×%d  (expect 4×32)\n', size(w1));
w2 = modwt(x);
fprintf('  modwt(x)             → size = %d×%d  (expect 6×32, sym4 default, lev=5)\n', size(w2));
w3 = modwt(x, 4);
fprintf('  modwt(x, 4)          → size = %d×%d  (expect 5×32, default sym4)\n\n', size(w3));

fprintf('=== swt + iswt round trip ===\n');
sc = swt(x, 3, 'haar');
xr = iswt(sc, 'haar');
fprintf('  max|iswt(swt(x)) - x| = %g  (expect ~0)\n\n', max(abs(xr(:) - x(:))));

fprintf('=== modwt + imodwt round trip ===\n');
w  = modwt(x, 'haar', 3);
xr = imodwt(w, 'haar');
fprintf('  max|imodwt(modwt(x)) - x| = %g  (expect ~0)\n\n', max(abs(xr(:) - x(:))));

fprintf('=== same with db2 wavelet ===\n');
w  = modwt(x, 'db2', 3);
xr = imodwt(w, 'db2');
fprintf('  modwt+imodwt db2: max diff = %g\n', max(abs(xr(:) - x(:))));
sc = swt(x, 2, 'db2');
xr = iswt(sc, 'db2');
fprintf('  swt+iswt db2:     max diff = %g\n', max(abs(xr(:) - x(:))));
