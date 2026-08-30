clear

% periodogram default NFFT is max(256, 2^nextpow2(N)) (MATLAB R2025b), so a
% short signal is zero-padded to at least a 256-point FFT (129 one-sided
% bins), not just 2^nextpow2(N). The PSD values are unchanged.

[p, f] = periodogram([1 2 1 2 1 2 1 2]);
fprintf('periodogram(8-sample)  numel(f)=%d  (expect 129)\n', numel(f));
fprintf('  p(1)=%.5f p(10)=%.5f f(10)=%.5f  (expect 2.86479, 4.40929, 0.22089)\n', ...
        p(1), p(10), f(10));

% 2^nextpow2(300) = 512 > 256 -> 257 one-sided bins
[p2, f2] = periodogram(sin(2*pi*(0:299)/10));
fprintf('periodogram(300-sample)  numel(f2)=%d  (expect 257)\n', numel(f2));

% explicit NFFT still honoured (not forced to 256)
[p3, f3] = periodogram([1 2 1 2 1 2 1 2], [], 8);
fprintf('periodogram(x,[],8)  numel(f3)=%d  (expect 5)\n', numel(f3));
