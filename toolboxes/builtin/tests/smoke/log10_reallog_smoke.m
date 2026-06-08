clear
import compat.*
% log10 / reallog smoke — now SIMD-backed (Highway hn::Log10 / hn::Log).
% Run: build/desktop-fast/tests/smoke/Release/numkit_smoke.exe \
%      toolboxes/builtin/tests/smoke/log10_reallog_smoke.m

fprintf('--- log10 exact at powers of 10 (vector hits SIMD body) ---\n');
p = log10([1 10 100 1000 1e4 1e5 1e6 1e7 1e8 1e9]);
fprintf('log10(10.^(0:9)):'); fprintf(' %g', p); fprintf('\n');
fprintf('  (expect 0 1 2 3 4 5 6 7 8 9, exact)\n');

fprintf('\n--- log10 general values ---\n');
fprintf('log10(2)     = %.17g (expect ~0.30102999566398120)\n', log10(2));
fprintf('log10(0.001) = %.17g (expect -3)\n', log10(0.001));
fprintf('log10(1234.5)= %.17g (expect ~3.0914910942679512)\n', log10(1234.5));

fprintf('\n--- reallog (= log on positive domain) ---\n');
fprintf('reallog(1)   = %.17g (expect 0)\n', reallog(1));
fprintf('reallog(e)   = %.17g (expect 1)\n', reallog(exp(1)));
fprintf('reallog(10)  = %.17g (expect ~2.3025850929940459)\n', reallog(10));

fprintf('\n--- reallog rejects negatives (MATLAB: use log) ---\n');
try
    reallog(-1);
    fprintf('NO ERROR (unexpected)\n');
catch
    fprintf('reallog(-1) threw as expected\n');
end
