clear

% xcov — cross-covariance with scaleopt + maxlag (MATLAB R2025b parity).
x = [1 3 -2 4 0];
y = [2 -1 0 3 1];

cn = xcov(x, y);            % 'none' (raw). Length 2N-1 = 9, zero-lag at idx 5.
fprintf('none    : c(5)=%.4f (expect 5)\n', cn(5));

cb = xcov(x, y, 'biased');  % divide every lag by N=5
fprintf('biased  : c(5)=%.4f c(4)=%.4f (expect 1  -1.56)\n', cb(5), cb(4));

cu = xcov(x, y, 'unbiased');% divide lag m by (N-|m|)
fprintf('unbiased: c(5)=%.4f c(3)=%.4f (expect 1  1.2667)\n', cu(5), cu(3));

cc = xcov(x, y, 'coeff');   % divide by sqrt(Cxx0*Cyy0)=sqrt(228)
fprintf('coeff   : c(5)=%.6f (expect 0.331133)\n', cc(5));

cm = xcov(x, y, 2);         % maxlag 2 -> length 5, lags -2..2
fprintf('maxlag2 : numel=%d c(3)=%.4f (expect 5  5)\n', numel(cm), cm(3));

cmb = xcov(x, y, 2, 'biased');
fprintf('maxlag2+biased: c(3)=%.4f c(1)=%.4f (expect 1  0.76)\n', cmb(3), cmb(1));
