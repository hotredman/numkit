clear

% Negative binomial: failures before r-th success.
% pmf(2; r=3, p=0.5) = C(4,2) * 0.5³ * 0.5² = 6 * 0.03125 = 0.1875
fprintf('nbinpdf(2, 3, 0.5)  = %.6f  (expect 0.187500)\n', nbinpdf(2, 3, 0.5));
fprintf('nbinpdf(0, 5, 0.4)  = %.6f  (expect 0.010240)\n', nbinpdf(0, 5, 0.4));

% cdf(2; 3, 0.5) = sum pmf(0..2) = 0.125 + 0.1875 + 0.1875 = 0.500
fprintf('nbincdf(2, 3, 0.5)  = %.6f  (expect 0.500000)\n', nbincdf(2, 3, 0.5));

% inv: round-trip
fprintf('nbininv(nbincdf(5, 4, 0.3), 4, 0.3) = %.6f  (expect 5)\n', ...
    nbininv(nbincdf(5, 4, 0.3), 4, 0.3));

% nbinstat(3, 0.4): mean = 3·0.6/0.4 = 4.5, var = 4.5/0.4 = 11.25
[m, v] = nbinstat(3, 0.4);
fprintf('nbinstat(3, 0.4)    = [%.4f, %.4f]  (expect [4.5, 11.25])\n', m, v);

% rnd
N = 50000;
X = nbinrnd(3, 0.4, N, 1);
fprintf('nbinrnd(3, 0.4) N=%d: mean=%.4f var=%.4f (expect 4.5, 11.25)\n', N, mean(X), var(X));
