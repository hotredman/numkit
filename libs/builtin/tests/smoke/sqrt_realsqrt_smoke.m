clear
import compat.*
% sqrt / realsqrt smoke — now SIMD-backed (Highway hn::Sqrt = vsqrtpd).
% Run: build/desktop-fast/tests/smoke/Release/numkit_smoke.exe \
%      libs/builtin/tests/smoke/sqrt_realsqrt_smoke.m

fprintf('--- sqrt vector (SIMD body + tail), perfect squares are exact ---\n');
s = sqrt([0 1 4 9 16 25 36 49 64 81]);
fprintf('sqrt(0,1,4,...,81):'); fprintf(' %g', s); fprintf('\n');
fprintf('  (expect 0 1 2 3 4 5 6 7 8 9)\n');
fprintf('sqrt(2) = %.17g (expect ~1.4142135623730951)\n', sqrt(2));

fprintf('\n--- realsqrt (= sqrt on nonnegative domain) ---\n');
fprintf('realsqrt(16) = %g (expect 4)\n', realsqrt(16));
rv = realsqrt([1 4 9 16 25 36 49 64 81 100]);
fprintf('realsqrt(...):'); fprintf(' %g', rv); fprintf('\n');
fprintf('  (expect 1 2 3 4 5 6 7 8 9 10)\n');

fprintf('\n--- realsqrt rejects negatives (MATLAB: use sqrt) ---\n');
try
    realsqrt(-1);
    fprintf('NO ERROR (unexpected)\n');
catch
    fprintf('realsqrt(-1) threw as expected\n');
end
