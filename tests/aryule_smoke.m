import compat.*

% --- aryule on AR(2) recovers coefficients ---
% True AR(2): y[n] - a1·y[n-1] - a2·y[n-2] = e[n]
% with a1 = 2*0.95*cos(pi/4), a2 = -0.95^2
rng(42);
a1_true = 2 * 0.95 * cos(pi/4);
a2_true = -0.95^2;
N = 4096;
e = randn(N + 200, 1);
y = zeros(N + 200, 1);
for n = 3:numel(y)
    y(n) = a1_true * y(n-1) + a2_true * y(n-2) + e(n);
end
x = y(201:end);

% MATLAB convention: aryule returns [1, a_1, a_2] where a satisfies
%   y[n] + a_1·y[n-1] + a_2·y[n-2] = e[n]
% so a_k_matlab = -a_k_true.
[a, ev] = aryule(x, 2);
fprintf('--- aryule(x, 2) on AR(2) ---\n');
fprintf('  a = [%.4f, %.4f, %.4f]  (expect [1, %.4f, %.4f])\n', ...
    a(1), a(2), a(3), -a1_true, -a2_true);
fprintf('  e (variance) = %.4f (expect ≈ 1.0 — driver was randn)\n\n', ev);

% --- lpc gives identical answer to aryule (alias) ---
[a_lpc, e_lpc] = lpc(x, 2);
fprintf('--- lpc(x, 2) ---\n');
fprintf('  max|a_lpc - a_aryule| = %.6e (expect 0)\n', max(abs(a_lpc - a)));
fprintf('  e_lpc - e_aryule       = %.6e (expect 0)\n', e_lpc - ev);
