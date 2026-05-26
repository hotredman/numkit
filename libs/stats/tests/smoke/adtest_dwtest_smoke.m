clear
import compat.*

fprintf('=== adtest — Anderson-Darling normality test ===\n');

rng(0);

% Normal sample.
xn = randn(300, 1);
[h, p, A2, cv] = adtest(xn);
fprintf('\nNormal data (n=300):\n');
fprintf('   h = %d,  p = %.4f,  A² = %.4f,  cv = %.3f\n', h, p, A2, cv);
fprintf('   (expect h = 0, fail to reject normality)\n');

% Exponential — heavily skewed.
xe = -log(rand(200, 1));
[h2, p2, A2_2, ~] = adtest(xe);
fprintf('\nExponential data (n=200):\n');
fprintf('   h = %d,  p = %.4f,  A² = %.4f\n', h2, p2, A2_2);
fprintf('   (expect h = 1, p ≈ 0)\n');

% Uniform — also non-normal.
xu = rand(200, 1);
[h3, p3, A2_3, ~] = adtest(xu);
fprintf('\nUniform data (n=200):\n');
fprintf('   h = %d,  p = %.4f,  A² = %.4f\n', h3, p3, A2_3);


fprintf('\n=== dwtest — Durbin-Watson autocorrelation test ===\n');

n = 200;
X = [ones(n, 1), (1:n)'];

% IID residuals.
r_iid = randn(n, 1);
[p_iid, dw_iid] = dwtest(r_iid, X);
fprintf('\nIID residuals (n=200):\n');
fprintf('   DW = %.4f,  p = %.4f  (DW ≈ 2 for IID)\n', dw_iid, p_iid);

% Strong positive autocorrelation.
r_pos = zeros(n, 1);
r_pos(1) = randn();
for i = 2:n
    r_pos(i) = 0.8 * r_pos(i - 1) + 0.2 * randn();
end
[p_pos, dw_pos] = dwtest(r_pos, X);
fprintf('\nPositively-autocorrelated residuals (φ = 0.8):\n');
fprintf('   DW = %.4f,  p = %.4f  (DW far below 2, p ≈ 0)\n', dw_pos, p_pos);

% Anti-autocorrelation.
r_neg = zeros(n, 1);
r_neg(1) = randn();
for i = 2:n
    r_neg(i) = -0.8 * r_neg(i - 1) + 0.2 * randn();
end
[p_neg, dw_neg] = dwtest(r_neg, X);
fprintf('\nAnti-autocorrelated residuals (φ = -0.8):\n');
fprintf('   DW = %.4f,  p = %.4f  (DW far above 2, p ≈ 0)\n', dw_neg, p_neg);
