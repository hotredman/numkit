clear
import compat.*
% str2num evaluates the (bracket-wrapped) string as a MATLAB expression:
% matrices, ranges, arithmetic. DEEP-PROBE 2026-05-31: numkit's str2num was
% scalar-only and returned [] for matrices/ranges. Pinned vs MATLAB R2025b.

m = str2num('[1 2; 3 4]');
fprintf('matrix %dx%d, m(2,1)=%g m(1,2)=%g  (expect 2x2, 3 2)\n', ...
        size(m,1), size(m,2), m(2,1), m(1,2));
fprintf('range 1:5 = '); disp(str2num('1:5'));        % 1 2 3 4 5
fprintf('expr 2+3 = %g  (expect 5)\n', str2num('2+3'));
fprintf('scalar 42.5 = %g\n', str2num('42.5'));
fprintf('bad isempty = %d  (expect 1)\n', isempty(str2num('not a number')));
[x, tf] = str2num('[10 20 30]');
fprintf('[x,tf]: tf=%d x(2)=%g  (expect 1, 20)\n', tf, x(2));
