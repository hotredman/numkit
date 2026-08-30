clear

% occupied bandwidth + [bw, flo, fhi, power] -- bugs/signal/obw-value-outputs.
% 99% occupied bandwidth via a rectangular-windowed periodogram (nfft = N, no
% zero-pad), rectangle-rule cumulative power, band edges where the cumulative
% reaches 0.5% and 99.5% of the total (frequencies at the bin upper edge F+df/2).

fs = 1000; t = (0:fs-1)/fs;
x = sin(2*pi*100*t) + 0.5*sin(2*pi*200*t);   % deterministic two-tone

[bw, flo, fhi, pw] = obw(x, fs);
fprintf('bw  = %.6f   (expect 100.968750)\n', bw);
fprintf('flo = %.6f   (expect  99.506250)\n', flo);
fprintf('fhi = %.6f   (expect 200.475000)\n', fhi);
fprintf('pw  = %.6f   (expect   0.618750 = 0.99 * total power)\n', pw);
fprintf('obw (1 output) = %.6f   (expect 100.968750)\n', obw(x, fs));
