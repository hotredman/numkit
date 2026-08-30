clear

fprintf('=== cond(A, p) — all p ===\n');

A = [1 0; 0 1e-3];
fprintf('A = diag(1, 1e-3):\n');
fprintf('  cond(A, 1)    = %.4e\n', cond(A, 1));
fprintf('  cond(A, 2)    = %.4e\n', cond(A, 2));
fprintf('  cond(A, Inf)  = %.4e\n', cond(A, Inf));
fprintf('  cond(A, fro)  = %.4e\n', cond(A, 'fro'));

fprintf('\nidentity (3x3):\n');
fprintf('  cond(eye(3), 1)   = %.4f\n', cond(eye(3), 1));
fprintf('  cond(eye(3), 2)   = %.4f\n', cond(eye(3), 2));
fprintf('  cond(eye(3), Inf) = %.4f\n', cond(eye(3), Inf));
fprintf('  cond(eye(3), fro) = %.4f   (== sqrt(3))\n', cond(eye(3), 'fro'));

% 'inf' string form ≡ Inf numeric.
fprintf('\n"inf" string = Inf numeric:\n');
A2 = [1 2; 3 4];
fprintf('  cond(A, "inf") = %.4f  cond(A, Inf) = %.4f  match=%d\n', ...
        cond(A2, 'inf'), cond(A2, Inf), cond(A2, 'inf') == cond(A2, Inf));

fprintf('\ndefault (no p) is p=2:\n');
fprintf('  cond(A) = %.4f  cond(A, 2) = %.4f\n', cond(A2), cond(A2, 2));

fprintf('\nsingular A → Inf:\n');
fprintf('  cond([1 1;1 1], 1) = %g\n', cond([1 1; 1 1], 1));
