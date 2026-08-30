clear

% Discrete uniform on {1..N}
fprintf('--- unid ---\n');
fprintf('unidpdf(3, 6)       = %.6f  (expect 0.166667)\n', unidpdf(3, 6));
fprintf('unidpdf(0, 6)       = %.6f  (expect 0.000000)\n', unidpdf(0, 6));
fprintf('unidcdf(3, 6)       = %.6f  (expect 0.500000)\n', unidcdf(3, 6));
fprintf('unidinv(0.5, 6)     = %.6f  (expect 3)\n', unidinv(0.5, 6));
fprintf('unidinv(0.95, 10)   = %.6f  (expect 10)\n', unidinv(0.95, 10));
[m, v] = unidstat(6);
fprintf('unidstat(6)         = [%.4f, %.4f]  (expect [3.5, 2.9167])\n', m, v);
N = 50000;
X = unidrnd(6, N, 1);
fprintf('unidrnd(6) N=%d: mean=%.4f var=%.4f (expect 3.5, 2.9167)\n', N, mean(X), var(X));

% Geometric
fprintf('--- geo (k = failures before 1st success) ---\n');
% pdf(0; 0.3) = 0.3 ; pdf(2; 0.3) = 0.7² · 0.3 = 0.147
fprintf('geopdf(0, 0.3)      = %.6f  (expect 0.300000)\n', geopdf(0, 0.3));
fprintf('geopdf(2, 0.3)      = %.6f  (expect 0.147000)\n', geopdf(2, 0.3));
% cdf(2; 0.3) = 1 - 0.7³ = 1 - 0.343 = 0.657
fprintf('geocdf(2, 0.3)      = %.6f  (expect 0.657000)\n', geocdf(2, 0.3));
% inv: round-trip
fprintf('geoinv(geocdf(5, 0.4), 0.4) = %.6f  (expect 5)\n', geoinv(geocdf(5, 0.4), 0.4));
% inv(0.5, 0.3): F(2)=0.657, F(1)=0.51 — so inv(0.5)=1
fprintf('geoinv(0.5, 0.3)    = %.6f  (expect 1)\n', geoinv(0.5, 0.3));
[m, v] = geostat(0.3);
fprintf('geostat(0.3)        = [%.4f, %.4f]  (expect [2.3333, 7.7778])\n', m, v);
X = geornd(0.3, N, 1);
fprintf('geornd(0.3) N=%d: mean=%.4f var=%.4f (expect 2.3333, 7.7778)\n', N, mean(X), var(X));
