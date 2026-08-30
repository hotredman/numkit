clear

fprintf('=== condest ===\n');

% Identity: 1.
fprintf('condest(eye(3))                 = %.4f  (expect 1)\n', condest(eye(3)));

% Diagonal: max|d|/min|d|.
fprintf('condest(diag([1 1e-3 1e-6]))    = %.4e  (expect 1e6)\n', condest(diag([1 1e-3 1e-6])));

% Small upper-triangular: from MATLAB R2025b == 14.
fprintf('condest([1 2 3; 0 4 5; 0 0 6])  = %.4f  (expect 14)\n', condest([1 2 3; 0 4 5; 0 0 6]));

% Singular: Inf.
fprintf('condest([1 1; 1 1])             = %.4f  (expect Inf)\n', condest([1 1; 1 1]));

% Hilbert matrix — KNOWN GAP: our exact value vs MATLAB's iterative
% estimate diverge slightly on near-singular inputs.
H = hilb(4);
fprintf('condest(hilb(4))                = %.4e  (MATLAB ≈ 2.84e+04 via Higham power-iter)\n', condest(H));
