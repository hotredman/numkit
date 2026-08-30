clear

% periodogram with a non-power-of-two nfft -- bugs/signal/periodogram-nonpow2-nfft.
% Previously fftRadix2 (power-of-two only) produced a garbage spectrum for a
% non-pow2 nfft (peak in the wrong bin, Parseval broken); now non-pow2 routes
% through the general fft (Bluestein). nfft = 1000 = N (no zero-pad).

fs = 1000; t = (0:fs-1)/fs;
x = sin(2*pi*100*t) + 0.5*sin(2*pi*200*t);
[P, F] = periodogram(x, [], 1000, fs);

fprintf('numel(P)=%d  F(end)=%g  df=%g   (expect 501, 500, 1)\n', numel(P), F(end), F(2)-F(1));
[mx, ix] = max(P);
fprintf('peak P=%.6f at f=%g   (expect 0.500000 at 100)\n', mx, F(ix));
fprintf('P(f=100)=%.6f  P(f=200)=%.6f   (expect 0.500000, 0.125000)\n', P(101), P(201));
fprintf('sum(P)*df=%.6f   (expect 0.625000 = mean(x^2))\n', sum(P)*(F(2)-F(1)));
