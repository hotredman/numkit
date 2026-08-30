clear

% ifft(X, ..., 'symmetric') (DEEP-PROBE 2026-05-31). Previously numkit threw
% "Cannot convert char to scalar" because it parsed 'symmetric' as the FFT
% length N. MATLAB treats X as conjugate-symmetric (lower half authoritative,
% conj-mirror to the upper half, DC/Nyquist forced real) and returns an
% EXACTLY REAL result — which differs from real(ifft(X)). vs MATLAB R2025b.

x = [1 2+1i 3 4-1i];

fprintf('=== vector ===\n');
v = ifft(x, 'symmetric');
fprintf('ifft(x,"symmetric") = [%g %g %g %g]  (expect [2 -1 0 0])\n', v(1), v(2), v(3), v(4));
fprintf('isreal = %d  (expect 1)\n', isreal(v));
fprintf('cf real(ifft(x))   = [%g %g %g %g]  (expect [2.5 -1 -0.5 0] — DIFFERENT)\n', ...
        real(ifft(x)));

fprintf('\n=== zero-pad to n=6 ===\n');
w = ifft(x, 6, 'symmetric');
fprintf('numel = %d (expect 6); w(1)=%g w(2)=%g w(6)=%g\n', numel(w), w(1), w(2), w(6));
fprintf('  expect 2.5  -0.955342  -0.377992\n');

fprintf('\n=== matrix per active dim ===\n');
M = [1 2+1i; 3+2i 4-1i];
A = ifft(M, 'symmetric');
fprintf('dim1 col-wise = [%g %g; %g %g]  (expect [2 3; -1 -1])\n', A(1,1), A(1,2), A(2,1), A(2,2));
B = ifft(M, [], 2, 'symmetric');
fprintf('dim2 row-wise = [%g %g; %g %g]  (expect [1.5 -0.5; 3.5 -0.5])\n', B(1,1), B(1,2), B(2,1), B(2,2));
