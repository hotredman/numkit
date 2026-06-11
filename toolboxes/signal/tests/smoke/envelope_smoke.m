clear

import compat.*

% envelope: full MATLAB R2025b parity across all four documented modes.
%
%   [yu, yl] = envelope(x)                  — FFT |hilbert(x-mean)| + mean
%   [yu, yl] = envelope(x, n, 'analytic')   — Kaiser(8) Hilbert FIR
%   [yu, yl] = envelope(x, n, 'rms')        — sliding-window RMS
%   [yu, yl] = envelope(x, n, 'peak')       — spline through findpeaks
%   yu       = envelope(x, n)               — same as 'analytic'
%
% All "expect" lines below were captured from MATLAB R2025b
% (verified bit-identical, see tools/parity/specs/envelope.json).

sig = sin(2*pi*0.1*(0:31)') .* exp(-0.05*(0:31)');

fprintf('=== envelope(sig) — default FFT analytic ===\n');
[up, lo] = envelope(sig);
fprintf('  up(1:6): '); disp(up(1:6)');
fprintf('  expect : 0.479438 0.825804 0.865036 0.898577 0.930393 0.881726\n');
fprintf('  lo(1:3): '); disp(lo(1:3)');
fprintf('  expect : -0.397409 -0.743775 -0.783007\n\n');

fprintf('=== envelope(sig, 8, ''analytic'') — Kaiser-tapered FIR ===\n');
[upa, loa] = envelope(sig, 8, 'analytic');
fprintf('  upa(1:6): '); disp(upa(1:6)');
fprintf('  expect  : 0.514194 0.808302 0.883341 0.747163 0.522220 0.523884\n\n');

fprintf('=== envelope(sig, 5, ''rms'') — sliding RMS ===\n');
[upr, lor] = envelope(sig, 5, 'rms');
fprintf('  upr(1:6): '); disp(upr(1:6)');
fprintf('  expect  : 0.601299 0.662779 0.630957 0.630957 0.623894 0.594715\n\n');

fprintf('=== envelope(sig, 1, ''peak'') — spline through findpeaks ===\n');
[upp, lop] = envelope(sig, 1, 'peak');
fprintf('  upp(1:6): '); disp(upp(1:6)');
fprintf('  expect  : 0.944259 0.901739 0.860552 0.820696 0.782173 0.744982\n\n');

fprintf('=== envelope(sig, 4) — short form == ''analytic'' ===\n');
upn = envelope(sig, 4);
fprintf('  upn(1:2): '); disp(upn(1:2)');
fprintf('  expect  : 0.409011 0.727105\n');
