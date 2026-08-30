clear

% conv shape arg as string ("...") vs char ('...') (DEEP-PROBE 2026-05-31).
% MATLAB R2025b accepts BOTH 'same' and "same" for the shape arg. numkit's
% conv_reg checked only isChar(), so a double-quoted shape was SILENTLY
% IGNORED and fell back to the full convolution. vs MATLAB R2025b.

a = [1 2 3 4]; b = [1 1];

fprintf('=== "same" (string) must match ''same'' (char) ===\n');
e = conv(a, b, "same");
fprintf('conv([1 2 3 4],[1 1],"same") = [%g %g %g %g]  (expect [3 5 7 4], len 4)\n', e(1), e(2), e(3), e(4));
fprintf('numel = %d  (expect 4, NOT 5 = full)\n', numel(e));

fprintf('\n=== "valid" (string) ===\n');
v = conv([1 2 3 4 5], [1 1 1], "valid");
fprintf('conv([1 2 3 4 5],[1 1 1],"valid") = [%g %g %g]  (expect [6 9 12], len 3)\n', v(1), v(2), v(3));
fprintf('numel = %d  (expect 3)\n', numel(v));

fprintf('\n=== "full" (string) explicit = default ===\n');
f = conv(a, b, "full");
fprintf('conv([1 2 3 4],[1 1],"full") numel = %d  (expect 5)\n', numel(f));
fprintf('f(1)=%g f(5)=%g  (expect 1 and 4)\n', f(1), f(5));
