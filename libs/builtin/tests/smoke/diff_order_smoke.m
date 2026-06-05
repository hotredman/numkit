clear
import compat.*

% diff's order N must be a positive integer scalar. Fixed 2026-06-05
% (bugs/builtin/diff-zero-order.md): N=0 used to return an identity copy.
% Reference: MATLAB R2025b.

a = diff([1 3 6 10 15]);
fprintf('diff([1 3 6 10 15])    = %g %g %g %g   (expect 2 3 4 5)\n', a(1),a(2),a(3),a(4));

b = diff([1 3 6 10 15], 2);
fprintf('diff(X, 2)             = %g %g %g       (expect 1 1 1)\n', b(1),b(2),b(3));

% N=0, negative, fractional and non-scalar orders are now errors.
bad = {0, -1, 1.5, [1 2]};
for k = 1:numel(bad)
    try
        diff([1 2 3], bad{k});
        fprintf('diff([1 2 3], %s): NO ERROR (unexpected)\n', mat2str(bad{k}));
    catch e
        fprintf('diff([1 2 3], %s) -> errors OK: %s\n', mat2str(bad{k}), e.message);
    end
end
