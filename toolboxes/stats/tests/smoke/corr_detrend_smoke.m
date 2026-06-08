clear
import compat.*

fprintf('=== corr (Pearson alias) ===\n');
X = [1 2; 2 4; 3 6; 4 8];   % perfectly correlated columns
fprintf('  corr([1 2; 2 4; 3 6; 4 8]):\n'); disp(corr(X));
fprintf('  expect [1 1; 1 1] (perfectly correlated)\n');

Xn = [1 4; 2 3; 3 2; 4 1];   % anti-correlated
fprintf('\n  corr([1 4; 2 3; 3 2; 4 1]):\n'); disp(corr(Xn));
fprintf('  expect off-diagonal = -1 (anti-correlated)\n');

fprintf('\n=== detrend ===\n');
y = (1:10)' * 2 + 5;  % strict linear: 7, 9, 11, ..., 25
yd = detrend(y);
fprintf('  y = 2x + 5; detrend(y) max abs = %g (expect ~0)\n', max(abs(yd)));

yq = ((1:10)').^2;  % strict quadratic
ydq = detrend(yq, 2);
fprintf('  y = x^2; detrend(y, 2) max abs = %g (expect ~0)\n', max(abs(ydq)));

% String mode
yc = [1 2 3 4 5];
yc_const = detrend(yc, 'constant');
fprintf('  detrend([1..5], ''constant'') = '); disp(yc_const);
fprintf('  expect [-2 -1 0 1 2] (subtract mean=3)\n');
