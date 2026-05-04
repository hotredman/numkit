import compat.*

% wblpdf(2, 3, 2): a=3 (scale), b=2 (shape)
% f = (b/a) (x/a)^(b-1) exp(-(x/a)^b)
%   = (2/3) (2/3)^1 exp(-(2/3)^2)
%   = (2/3) * 0.6667 * exp(-0.4444) = 0.4444 * 0.6412 = 0.28494
fprintf('wblpdf(2, 3, 2)     = %.6f  (expect 0.284938)\n', wblpdf(2, 3, 2));

% wblpdf(1, 1, 1) — exp(-1) ≈ 0.3679
fprintf('wblpdf(1, 1, 1)     = %.6f  (expect 0.367879)\n', wblpdf(1, 1, 1));

% wblcdf(2, 3, 2) = 1 - exp(-(2/3)²) = 1 - 0.6412 = 0.3588
fprintf('wblcdf(2, 3, 2)     = %.6f  (expect 0.358821)\n', wblcdf(2, 3, 2));

% wblinv: round-trip
fprintf('wblinv(wblcdf(2.5)) = %.6f  (expect 2.500000)\n', wblinv(wblcdf(2.5, 3, 2), 3, 2));

% wblstat(3, 2): mean = 3·Γ(1.5) = 3·0.8862 ≈ 2.6587, var = 9·(1 - π/4) = 9·0.2146 = 1.9311
[m, v] = wblstat(3, 2);
fprintf('wblstat(3, 2)       = [%.4f, %.4f]  (expect [2.6587, 1.9311])\n', m, v);

% rnd
N = 50000;
X = wblrnd(3, 2, N, 1);
fprintf('wblrnd(3,2) N=%d: mean=%.4f var=%.4f (expect 2.6587, 1.9311)\n', N, mean(X), var(X));
