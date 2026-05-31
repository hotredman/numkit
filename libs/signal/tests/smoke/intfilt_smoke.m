clear
import compat.*

% intfilt(R, L, freqmult) designs a bandlimited interpolation FIR filter of
% length 2*R*L-1. MATLAB R2025b builds it by LEAST SQUARES: it is exactly
% firls(2*R*L-2, F*2, M) over a piecewise band/amplitude spec (passband
% amplitude R around the image-free bands, zero in the spectral images).
% numkit previously used a Hamming-windowed sinc whose coefficients
% disagreed with MATLAB; it now reuses firls and matches bit-for-bit.

b = intfilt(4, 2, 0.5);
fprintf('--- intfilt(4, 2, 0.5) ---\n');
fprintf('length=%d  b(1)=%.10f  b(8)=%.10f (centre)  sum=%.10f\n', ...
        numel(b), b(1), b(8), sum(b));
fprintf('  (expect 15, -0.0582705792, 1.0, 3.9678094037)\n');

c = intfilt(3, 4, 0.6);
fprintf('--- intfilt(3, 4, 0.6) ---\n');
fprintf('length=%d  c(1)=%.10f  c(12)=%.10f\n', numel(c), c(1), c(12));
fprintf('  (expect 23, -0.0089374149, 1.0)\n');

% freqmult = 1: the [R R 0 0] band special case.
d = intfilt(2, 2, 1);
fprintf('--- intfilt(2, 2, 1) ---\n');
fprintf('length=%d  d(1)=%.10f  d(4)=%.10f\n', numel(d), d(1), d(4));
fprintf('  (expect 7, -0.2122065908, 1.0)\n');

% The filter is linear-phase (symmetric).
fprintf('symmetric: max|b - flip(b)| = %.2e (expect 0)\n', max(abs(b - b(end:-1:1))));
