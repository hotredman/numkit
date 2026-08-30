clear

% spectrogram 4th output (ps = power spectral density). DEEP-PROBE 2026-06:
% ps was missing ("Too many output arguments"). It is the one-sided PSD
% ps[k] = c[k]*|s[k]|^2 / (fs*sum(win.^2)), c = 2 interior, 1 at DC/Nyquist.
% Reference: MATLAB R2025b.

[s, f, t, ps] = spectrogram((1:64)', 8, 4, 16, 100);
fprintf('size(ps) = %dx%d\n', size(ps,1), size(ps,2));
fprintf('ps(1,1) = %.5f  (expect 1.08212, DC bin)\n', ps(1,1));
fprintf('ps(3,2) = %.5f  (expect 1.98718, interior, doubled)\n', ps(3,2));
fprintf('sum(ps) = %.2f  (expect 3254.62)\n', sum(ps(:)));
fprintf('min(ps) >= 0: %d  (expect 1)\n', min(ps(:)) >= 0);

% Default window / no fs.
[s2, f2, t2, ps2] = spectrogram((1:50)');
fprintf('default-window ps2: %dx%d, ps2(2,2) = %.3f  (expect 129xM, 344.941)\n', ...
        size(ps2,1), size(ps2,2), ps2(2,2));
