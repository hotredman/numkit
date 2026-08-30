clear

fprintf('=== polyeig ===\n');

% Linear: (A0 + λI)x = 0 → e = -eigvals(A0).
e_lin = polyeig([2 0; 0 3], eye(2));
fprintf('  linear (A0 + λI), A0 = diag(2,3): e (sorted) = ');
fprintf('%.3f ', sort(real(e_lin)));
fprintf('  (expect -3, -2)\n');

% Quadratic: (λ²-5λ+6)·I → e = {2, 2, 3, 3}.
e_q = polyeig(6*eye(2), -5*eye(2), eye(2));
fprintf('  quadratic (λ²-5λ+6)·I: real(e) = ');
fprintf('%.3f ', sort(real(e_q)));
fprintf('  (expect 2, 2, 3, 3)\n');

% Quadratic complex: (Iλ² + diag(4,9))*x = 0 → λ = ±2i, ±3i.
e_cplx = polyeig(diag([4 9]), zeros(2), eye(2));
fprintf('  complex case (eigvals ±2i, ±3i):\n');
for k = 1:numel(e_cplx)
    fprintf('    %.4f%+.4fi\n', real(e_cplx(k)), imag(e_cplx(k)));
end

fprintf('\n=== ordeig ===\n');

% Diagonal: order preserved (NO sort).
e_d = ordeig(diag([3 1 2]));
fprintf('  diag([3 1 2]): e = ['); fprintf('%.0f ', e_d); fprintf(']   (expect 3 1 2, NO sort)\n');

% Real Schur with 2×2 block.
T = [5 0 0; 0 0.5 -1.5; 0 1.5 0.5];
e_T = ordeig(T);
fprintf('  real Schur (block at (2,3)):\n');
for k = 1:numel(e_T)
    fprintf('    e(%d) = %.3f%+.3fi\n', k, real(e_T(k)), imag(e_T(k)));
end
