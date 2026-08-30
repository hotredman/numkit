clear

% levinson — Levinson-Durbin recursion (MATLAB R2025b parity).
% Branch 1: valid PSD autocorrelation -> standard AR fit.
[a, e, k] = levinson([1 0.6 0.3 0.1], 3);
fprintf('PSD   : a(2)=%.6f e=%.6f k(1)=%.3f (expect -0.650246  0.631773  -0.6)\n', a(2), e, k(1));

% Branch 2: NON-PSD autocorrelation (|k(2)|>1). MATLAB runs the full
% recursion through negative residual energy — it does NOT bail out.
[a2, e2, k2] = levinson([4 -2 -3 1 1.5], 3);
fprintf('nonPSD: a=[1 %.4f %.4f %.4f] e=%.4f k(3)=%.4f\n', a2(2), a2(3), a2(4), e2, k2(3));
fprintf('        expect a=[1 -1.7857 -1.25 -2.2143] e=9.1071 k(3)=-2.2143\n');
