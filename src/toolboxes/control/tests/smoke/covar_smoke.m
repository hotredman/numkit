clear

% covar: steady-state output (P) + state (Q) covariance under white noise W.
% bugs/control/covar. Q solves the gramian Lyapunov eqn (B*W*B'); P=C*Q*C'.

% 1st-order 1/(s+1): closed form P = B^2 W /(2|a|) * C^2 = 1/(2*1) = 0.5.
P = covar(ss(-1, 1, 1, 0), 1);
fprintf('covar 1/(s+1), W=1: P=%.6f  (expect 0.5)\n', P);

% 2-state, W=1: P = C*Q*C' = 1.416667.
[P, Q] = covar(ss([-1 0; 0 -2], [1; 1], [1 1], 0), 1);
fprintf('covar 2-state: P=%.6f  (expect 1.416667)\n', P);
fprintf('  Q = [%.4f %.4f; %.4f %.4f]  (expect [0.5 0.3333; 0.3333 0.25])\n', ...
        Q(1,1), Q(1,2), Q(2,1), Q(2,2));

% noise intensity scales P linearly: W=4 -> 4x.
P4 = covar(ss(-1, 1, 1, 0), 4);
fprintf('covar W=4: P=%.6f  (expect 2.0)\n', P4);

% discrete: A Q A' - Q + B W B' = 0; P = C Q C' (+ D W D').
Pd = covar(ss([0.5 0; 0 0.3], [1; 1], [1 1], 0, 0.1), 2);
fprintf('discrete: P=%.6f  (expect 9.570351)\n', Pd);
