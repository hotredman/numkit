clear
import compat.*

fprintf('=== signal/stft + istft — short-time Fourier transform ===\n');

x = sin(2*pi*0.05*(0:511));
w = 0.5*(1 - cos(2*pi*(0:63)/64));  % hann(64, 'periodic')

fprintf('\n[twosided STFT of sin@0.05cycles/sample, hann(64), 50%% overlap]\n');
s = stft(x, 'Window', w, 'OverlapLength', 32, 'FFTLength', 64);
fprintf('  size(s) = [%d %d]  (expect [64 15])\n', size(s, 1), size(s, 2));
fprintf('  s(1, 1)   = %+.6f %+.6fi   (expect -0.119014 + 0i)\n', ...
    real(s(1, 1)), imag(s(1, 1)));
fprintf('  s(33, 1)  = %+8.2e %+.6fi  (expect 6.77e-05 + 0i — DC bin)\n', ...
    real(s(33, 1)), imag(s(33, 1)));

fprintf('\n[onesided STFT — truncation to NFFT/2+1 rows]\n');
so = stft(x, 'Window', w, 'OverlapLength', 32, 'FFTLength', 64, ...
    'FrequencyRange', 'onesided');
fprintf('  size(so) = [%d %d]  (expect [33 15])\n', size(so, 1), size(so, 2));

fprintf('\n[istft round-trip identity]\n');
xr = real(istft(s, 'Window', w, 'OverlapLength', 32, 'FFTLength', 64));
fprintf('  length(xr) = %d  (expect 512)\n', length(xr));
err = max(abs(x(64:end-64) - xr(64:end-64)'));
fprintf('  inner max-err = %.3e  (expect ~ulp)\n', err);

fprintf('\nBit-equal (~ulp) with MATLAB R2025b. Octave 11.1.0 ships stft\n');
fprintf('in the signal package; current Octave does not accept the\n');
fprintf('FrequencyRange argument, so the harness reports N/A there.\n');
fprintf('KNOWN GAPS: centered FrequencyRange (MATLAB applies a per-bin\n');
fprintf('phase ramp — deferred), fs / time-axis multi-output, and\n');
fprintf('multi-channel matrix input.\n');
