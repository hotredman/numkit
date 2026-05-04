import compat.*

% --- 1st-order: G(s) = 1/(s+a). Closed-loop pole moves from -a (k=0)
%     toward -∞ along the real axis as k → ∞. ---
G = tf(1, [1 2]);
[r, k] = rlocus(G, [0 1 5 100]);
fprintf('--- rlocus(1/(s+2), [0 1 5 100]) ---\n');
fprintf('  size(r) = %dx%d\n', size(r, 1), size(r, 2));
fprintf('  pole at k=0   = %.4f (expect -2)\n', real(r(1, 1)));
fprintf('  pole at k=1   = %.4f (expect -3)\n', real(r(2, 1)));
fprintf('  pole at k=5   = %.4f (expect -7)\n', real(r(3, 1)));
fprintf('  pole at k=100 = %.4f (expect -102)\n\n', real(r(4, 1)));

% --- 2nd-order: G = 1/(s² + 2s + 1) with negative feedback gain k.
%     CL char poly: s² + 2s + (1+k). Poles: -1 ± sqrt(-k) for k > 0
%     (purely complex for k > 0, real for k = 0).
G2 = tf(1, [1 2 1]);
[r2, k2] = rlocus(G2, [0 1 4 9]);
fprintf('--- rlocus(1/(s²+2s+1), [0 1 4 9]) ---\n');
fprintf('  pole pair at k=0: %.4f, %.4f (expect -1, -1)\n', ...
    real(r2(1, 1)), real(r2(1, 2)));
fprintf('  pole at k=1: real=%.4f imag=%.4f (expect -1 ± 1j)\n', ...
    real(r2(2, 1)), abs(imag(r2(2, 1))));
fprintf('  pole at k=4: real=%.4f imag=%.4f (expect -1 ± 2j)\n', ...
    real(r2(3, 1)), abs(imag(r2(3, 1))));
fprintf('  pole at k=9: real=%.4f imag=%.4f (expect -1 ± 3j)\n\n', ...
    real(r2(4, 1)), abs(imag(r2(4, 1))));

% --- Default sweep (no k vector): 101 points (0 + 100 log-spaced) ---
[r3, k3] = rlocus(G);
fprintf('--- rlocus default sweep ---\n');
fprintf('  numel(k) = %d (expect 101)\n', numel(k3));
fprintf('  k(1)   = %.4f (expect 0)\n', k3(1));
fprintf('  k(end) = %.0f (expect 1000 — log10 = 3)\n', k3(end));

% --- Stable LTI plant feedback: T = G/(1+kG). Verify pole(T) ≈ rlocus row ---
% G = 1/(s+1), k = 5: T = 1/(s+1+5*1) = 1/(s+6). Pole at -6.
T = feedback(tf(1, [1 1]), tf(5, [1]));   % feedback with constant gain k=5
p_T = pole(T);
[r_check, k_check] = rlocus(tf(1, [1 1]), 5);
fprintf('\n--- compose feedback + rlocus ---\n');
fprintf('  pole(feedback(G, 5)) = %.4f (expect -6)\n', real(p_T(1)));
fprintf('  rlocus(G, 5)         = %.4f (expect -6)\n', real(r_check(1, 1)));
fprintf('  match diff = %.6e\n', abs(p_T(1) - r_check(1, 1)));
