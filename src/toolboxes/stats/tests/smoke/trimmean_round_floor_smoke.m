clear

% trimmean(X, percent[, flag][, dim]) — DEEP-PROBE 2026-05-31.
% Two MATLAB-parity fixes:
%  (a) the DEFAULT rounding of the per-end trim count was floor, but
%      MATLAB's default is 'round' (round n*percent/200 to the nearest
%      integer with ties going DOWN -> k = ceil(n*percent/200 - 0.5));
%  (b) the 'round'/'floor' flag arg was unparsed (threw "Cannot convert
%      char to scalar").
% w = [1 2 4 8 16 32 64 128 256 1000] (n=10) makes each per-end trim
% count k give a distinct mean (k0=151.1, k1=63.75, k2=42, k3=30).
% Reference: MATLAB R2025b.

w = [1 2 4 8 16 32 64 128 256 1000];

fprintf('=== p=35 (k_frac=1.75) ===\n');
fprintf('default = %.5f   (expect 42 — round to k=2)\n', trimmean(w, 35));
fprintf('round   = %.5f   (expect 42)\n', trimmean(w, 35, 'round'));
fprintf('floor   = %.5f   (expect 63.75 — k=1)\n', trimmean(w, 35, 'floor'));

fprintf('\n=== round-half-DOWN ties ===\n');
fprintf('p=30 (k_frac=1.5) default = %.5f   (expect 63.75 — rounds DOWN to k=1)\n', trimmean(w, 30));
fprintf('p=50 (k_frac=2.5) default = %.5f   (expect 42 — rounds DOWN to k=2)\n', trimmean(w, 50));

fprintf('\n=== flag + dim together ===\n');
M = [1 2; 3 4; 5 6; 7 8; 9 100];
tf = trimmean(M, 40, 'floor', 2);
fprintf('trimmean(M,40,''floor'',2) row 5 = %g   (expect 54.5)\n', tf(5));
