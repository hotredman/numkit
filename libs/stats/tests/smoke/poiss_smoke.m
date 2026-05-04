import compat.*

% poisspdf(0, 2) = exp(-2) = 0.1353
% poisspdf(2, 2) = 4·exp(-2)/2 = 2·exp(-2) = 0.2707
% poisspdf(5, 4) = 1024·exp(-4)/120 = 0.156293
fprintf('poisspdf(0, 2)      = %.6f  (expect 0.135335)\n', poisspdf(0, 2));
fprintf('poisspdf(2, 2)      = %.6f  (expect 0.270671)\n', poisspdf(2, 2));
fprintf('poisspdf(5, 4)      = %.6f  (expect 0.156293)\n', poisspdf(5, 4));

% poisscdf(2, 2) = pdf(0)+pdf(1)+pdf(2) = e^-2 (1 + 2 + 2) = 5/e² = 0.6767
fprintf('poisscdf(2, 2)      = %.6f  (expect 0.676676)\n', poisscdf(2, 2));
% poisscdf(0, 1) = e^-1 = 0.3679
fprintf('poisscdf(0, 1)      = %.6f  (expect 0.367879)\n', poisscdf(0, 1));

% poissinv: smallest k such that F(k;λ) ≥ p
% poissinv(0.5, 4) — median of Poisson(4) ≈ 4
fprintf('poissinv(0.5, 4)    = %.6f  (expect 4)\n', poissinv(0.5, 4));
% poissinv(0.95, 10) = 15 (MATLAB)
fprintf('poissinv(0.95, 10)  = %.6f  (expect 15)\n', poissinv(0.95, 10));
% Round-trip: cdf -> inv
fprintf('poissinv(poisscdf(7, 5), 5) = %.6f  (expect 7)\n', poissinv(poisscdf(7, 5), 5));

% poisstat: [λ, λ]
[m, v] = poisstat(7);
fprintf('poisstat(7)         = [%.4f, %.4f]  (expect [7, 7])\n', m, v);

% rnd
N = 50000;
X = poissrnd(5, N, 1);
fprintf('poissrnd(5) N=%d: mean=%.4f var=%.4f (expect 5, 5)\n', N, mean(X), var(X));
