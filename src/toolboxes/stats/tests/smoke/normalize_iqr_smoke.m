clear

% normalize's 'scale','iqr' and 'medianiqr' methods compute the interquartile
% range. They previously used the R-type-7 (n-1)*p linear-interpolation
% quantile rule, which gives a DIFFERENT IQR than MATLAB for small n. MATLAB
% (and numkit's own iqr/prctile) use the prctile (k-0.5)/n convention. For
% [1 2 4 8 16 32]: prctile Q1=2, Q3=16, IQR=14 (NOT the old 2.5/14/11.5).

x = [1 2 4 8 16 32];

fprintf('--- standalone iqr/prctile (already correct, unchanged) ---\n');
fprintf('iqr(x)=%.4f  prctile25=%.4f  prctile75=%.4f   (expect 14, 2, 16)\n', ...
        iqr(x), prctile(x, 25), prctile(x, 75));

fprintf('--- scale by IQR ---\n');
si = normalize(x, 'scale', 'iqr');
fprintf('si(1)=%.6f   (expect 1/14 = 0.071429)\n', si(1));

fprintf('--- medianiqr (center by median, scale by IQR) ---\n');
mi = normalize(x, 'medianiqr');
fprintf('mi(1)=%.6f   (expect (1-6)/14 = -0.357143)\n', mi(1));

fprintf('--- odd n = 5 ([1..5]: IQR = 2.5) ---\n');
s5 = normalize([1 2 3 4 5], 'scale', 'iqr');
fprintf('s5(1)=%.6f   (expect 1/2.5 = 0.4)\n', s5(1));

fprintf('--- per-column on a matrix ---\n');
M = [1 10; 2 20; 4 30; 8 40; 16 50; 32 60];
mm = normalize(M, 'medianiqr');
fprintf('m11=%.6f  m12=%.6f   (expect -0.357143, -0.833333)\n', mm(1,1), mm(1,2));
