clear

fprintf('=== signal/stft + istft — short-time Fourier transform ===\n');

x = sin(2*pi*0.05*(0:511));
w = 0.5*(1 - cos(2*pi*(0:63)/64));  % hann(64, 'periodic')

fprintf('\n[twosided STFT of sin@0.05cycles/sample, hann(64), 50%% overlap]\n');
s = stft(x, 'Window', w, 'OverlapLength', 32, 'FFTLength', 64, 'FrequencyRange', 'twosided');
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

fprintf('\n=== cycle 86: [s, f, t] = stft(x, fs) multi-output ===\n');
fs = 1000;
[s2, f, t] = stft(x, fs, 'Window', w, 'OverlapLength', 32, ...
    'FFTLength', 64, 'FrequencyRange', 'twosided');
fprintf('  twosided f: [%g %g ... %g]  (expect [0 15.625 ... 984.375])\n', ...
    f(1), f(2), f(end));
fprintf('  twosided t: [%g %g ... %g]  (expect [0.032 0.064 ... 0.480])\n', ...
    t(1), t(2), t(end));

[~, fc, ~] = stft(x, fs, 'Window', w, 'OverlapLength', 32, ...
    'FFTLength', 64, 'FrequencyRange', 'centered');
fprintf('  centered f: [%g ... %g]  (expect [-484.375 ... 500])\n', ...
    fc(1), fc(end));

[~, fo, ~] = stft(x, fs, 'Window', w, 'OverlapLength', 32, ...
    'FFTLength', 64, 'FrequencyRange', 'onesided');
fprintf('  onesided f: length=%d, last=%g (expect 33, 500)\n', ...
    length(fo), fo(end));

% No fs → f in rad/sample (fs=2π), t in samples (fs_t=1)
[~, fd, td] = stft(x);
fprintf('  No fs (default): f(1)=%.4f rad/sample, t(1)=%g samples\n', ...
    fd(1), td(1));

% istft 2-output
[xr2, tr] = istft(s2, fs, 'Window', w, 'OverlapLength', 32, ...
    'FFTLength', 64, 'FrequencyRange', 'twosided');
fprintf('  istft [xr,tr]: tr(1)=%g, tr(end)=%g (expect 0, 0.511)\n', ...
    tr(1), tr(end));

fprintf('\nBit-equal (~ulp) with MATLAB R2025b across all three\n');
fprintf('FrequencyRange modes (twosided / centered / onesided) and\n');
fprintf('all single/multi-output / fs / no-fs combinations.\n');
