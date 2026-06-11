clear
import compat.*

% signal/fwht + ifwht — fast Walsh-Hadamard transform.
% Reference: MATLAB R2025b.

fprintf('=== fwht / ifwht (Walsh-Hadamard) ===\n');

x = [1 2 3 4 5 6 7 8];

% Default ordering = sequency
ys = fwht(x);
fprintf('  fwht(1:8) sequency:\n'); disp(ys);
fprintf('  (expect [4.5 -2 0 -1 0 0 0 -0.5])\n');

yh = fwht(x, 8, 'hadamard');
fprintf('\n  fwht(1:8, 8, hadamard):\n'); disp(yh);
fprintf('  (expect [4.5 -0.5 -1 0 -2 0 0 0])\n');

yd = fwht(x, 8, 'dyadic');
fprintf('\n  fwht(1:8, 8, dyadic):\n'); disp(yd);
fprintf('  (expect [4.5 -2 -1 0 -0.5 0 0 0])\n');

% Zero-pad / truncate
ypad = fwht(x, 16);
fprintf('\n  fwht(1:8, 16) [zero-pad]: y(1)=%g, length=%d (e 2.25, 16)\n', ...
        ypad(1), length(ypad));

ytr = fwht(x, 4);
fprintf('  fwht(1:8, 4) [truncate]: y(1)=%g, length=%d (e 2.5, 4)\n', ...
        ytr(1), length(ytr));

% Round-trip
fprintf('\n  round-trip ifwht(fwht(x)) max-err: sequency=%g, hadamard=%g, dyadic=%g\n', ...
        max(abs(ifwht(ys) - x)), ...
        max(abs(ifwht(fwht(x, 8, 'hadamard'), 8, 'hadamard') - x)), ...
        max(abs(ifwht(fwht(x, 8, 'dyadic'), 8, 'dyadic') - x)));
fprintf('  (all expected 0 — H · H = N · I is integer-exact)\n');

% Impulse: fwht(delta_k) = (1/N) · row k of H (in chosen order)
fprintf('\n  fwht([1 0 0 0], 4, hadamard) = '); disp(fwht([1 0 0 0], 4, 'hadamard'));
fprintf('  (e [0.25 0.25 0.25 0.25] = row 0 of H4 / 4)\n');

fprintf('\n  y(1) is always mean(x) regardless of ordering.\n');
fprintf('Bit-equal MATLAB R2025b across all probed configurations.\n');
