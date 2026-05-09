clear
import compat.*

rng(42);
x = randn(100, 1);

fprintf('=== bootstrp ===\n');
B = bootstrp(1000, @mean, x);
fprintf('  shape: [%d %d] (expect [1000 1])\n', size(B,1), size(B,2));
fprintf('  mean(B) = %g  (close to mean(x) = %g)\n', mean(B), mean(x));
fprintf('  std(B) = %g  (≈ std(x)/sqrt(100) = %g, CLT)\n', std(B), std(x)/sqrt(100));

fprintf('\n=== bootci ===\n');
ci95 = bootci(1000, @mean, x);
fprintf('  95%% CI for mean: [%g, %g]\n', ci95(1), ci95(2));
fprintf('    contains true mean(x)=%g: %d\n', mean(x), ci95(1) <= mean(x) && mean(x) <= ci95(2));

ci90 = bootci(1000, @mean, x, 0.10);
ci99 = bootci(1000, @mean, x, 0.01);
fprintf('  90%% width = %g\n', ci90(2) - ci90(1));
fprintf('  95%% width = %g\n', ci95(2) - ci95(1));
fprintf('  99%% width = %g (widest, most conservative)\n', ci99(2) - ci99(1));

% Multi-output bootfun (e.g. mean + std)
fprintf('\n=== bootci with vector statistic [mean, std] ===\n');
ci_ms = bootci(1000, @(s) [mean(s), std(s)], x);
fprintf('  mean CI: [%g, %g]\n', ci_ms(1, 1), ci_ms(2, 1));
fprintf('  std CI:  [%g, %g]\n', ci_ms(1, 2), ci_ms(2, 2));
