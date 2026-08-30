clear

% [B,G] = sgolay(order, framelen) — the SECOND output G (DEEP-PROBE
% 2026-05-31). G is the framelen x (order+1) differentiation-filter matrix
% G = V*(V'V)^-1. Previously numkit returned only B, so [b,g]=sgolay(...)
% errored "Undefined variable g". G(:,1) is the smoothing filter (= central
% row of B); factorial(j-1)*G(:,j) is the (j-1)-th derivative filter.
% vs MATLAB R2025b.

[B, G] = sgolay(3, 5);
fprintf('=== [B,G] = sgolay(3,5) ===\n');
fprintf('B is %dx%d, G is %dx%d  (expect 5x5 and 5x4)\n', ...
        size(B,1), size(B,2), size(G,1), size(G,2));

fprintf('\nG(:,1) smoothing  = [%g %g %g %g %g]\n', ...
        G(1,1), G(2,1), G(3,1), G(4,1), G(5,1));
fprintf('  expect [-3 12 17 12 -3]/35 = [%g %g %g %g %g]\n', ...
        -3/35, 12/35, 17/35, 12/35, -3/35);

fprintf('\nG(:,2) 1st-deriv  = [%g %g %g %g %g]\n', ...
        G(1,2), G(2,2), G(3,2), G(4,2), G(5,2));
fprintf('  expect [1 -8 0 8 -1]/12 = [%g %g %g %g %g]\n', ...
        1/12, -8/12, 0, 8/12, -1/12);

fprintf('\nG(:,1) equals the central row of B? max|diff| = %g (expect ~0)\n', ...
        max(abs(G(:,1) - B(3,:)')));
