clear

% bilinear prewarp bilinear(b,a,fs,fp) (DEEP-PROBE 2026-05-31). With a
% prewarp frequency fp, the bilinear scale is K = 2*pi*fp/tan(pi*fp/fs) so
% the analog response at fp maps exactly onto the digital response at fp.
% numkit was DOUBLE-scaling K (2x too big). vs MATLAB R2025b.

fprintf('=== H(s)=s/(s+1)^2, fs=10, prewarp fp=2 ===\n');
[bp, ap] = bilinear([1 0], [1 2 1], 10, 2);
fprintf('bz = [%.7f %.7f %.7f]  (expect [0.0516691 0 -0.0516691])\n', bp(1), bp(2), bp(3));
fprintf('az = [%.6f %.6f %.6f]  (expect [1 -1.781374 0.793324])\n', ap(1), ap(2), ap(3));

fprintf('\n=== no prewarp (K = 2*fs = 20) unchanged ===\n');
[bn, an] = bilinear([1 0], [1 2 1], 10);
fprintf('bz(1) = %.7f  (expect 0.0453515)\n', bn(1));
