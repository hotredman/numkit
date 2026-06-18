clear
import compat.*

% Econometrics sample-correlation trio piece: autocorr (ACF) + crosscorr (CCF).
% bugs/stats/autocorr. Biased estimator c(k)=(1/N) sum (y-ybar)(y_{+k}-ybar),
% normalised to a correlation (lag-0 ACF == 1); confidence bounds +-2/sqrt(N).

[acf, lags, bounds] = autocorr([1 2 3 2 1 2 3 2 1]);
fprintf('autocorr: numel=%d   (expect 9 = min(20,N-1)+1)\n', numel(acf));
fprintf('  acf(1:3) = %.6f %.6f %.6f   (expect 1.000000 0.020202 -0.800505)\n', acf(1), acf(2), acf(3));
fprintf('  bounds   = %.6f %.6f   (expect +-0.666667)\n', bounds(1), bounds(2));

[xcf, xl] = crosscorr([1 2 3 4], [4 3 2 1], 'NumLags', 2);
fprintf('crosscorr: numel=%d   (expect 5)\n', numel(xcf));
fprintf('  lags = %g..%g\n', xl(1), xl(end));
fprintf('  xcf  = %.4f %.4f %.4f %.4f %.4f   (expect 0.30 -0.25 -1.00 -0.25 0.30)\n', ...
        xcf(1), xcf(2), xcf(3), xcf(4), xcf(5));
