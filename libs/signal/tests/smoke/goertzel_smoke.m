clear

import compat.*

% goertzel — single-bin DFT via 2nd-order IIR.
%
% Audit ТЗ closure 2026-05-09:
%   Pre-fix: numkit's adapter required (x, ind) and threw on the
%   1-arg form. After the fix, goertzel(x) defaults ind = 1:N
%   (full DFT) per MATLAB R2025b.

sig = sin(2*pi*0.1*(0:31)') .* exp(-0.05*(0:31)');

fprintf('=== goertzel(sig, [5 15]) — partial bins ===\n');
y = goertzel(sig, [5 15]);
fprintf('  y(1) = %g + %gj   (expect MATLAB -2.7057 + -0.2408j)\n', real(y(1)), imag(y(1)));
fprintf('  y(2) = %g + %gj   (expect MATLAB -0.2577 + 0.0194j)\n\n', real(y(2)), imag(y(2)));

fprintf('=== goertzel(sig) — 1-arg form, full DFT (was THROWING) ===\n');
yfull = goertzel(sig);
fprintf('  numel = %d  (expect 32)\n', numel(yfull));
fprintf('  yfull(1) = %g + %gj   (expect DC ~1.3125)\n\n', real(yfull(1)), imag(yfull(1)));

fprintf('=== consistency: full-DFT bin equals partial-bin call ===\n');
y5  = goertzel(sig, 5);
fprintf('  yfull(5)         = %g + %gj\n', real(yfull(5)), imag(yfull(5)));
fprintf('  goertzel(sig, 5) = %g + %gj\n', real(y5), imag(y5));
fprintf('  diff             = %g  (expect ~0)\n', abs(yfull(5) - y5));
