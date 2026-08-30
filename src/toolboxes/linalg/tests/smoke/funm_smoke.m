clear

% funm(A, fun) — general matrix function: scalar `fun` OF a matrix (not
% element-wise), via F = V*diag(fun(diag(D)))/V where [V,D] = eig(A).
% bugs/linalg/funm.

% Diagonal matrix: closed form funm(diag(d), fun) = diag(fun(d)).
F = funm([2 0; 0 3], @exp);
fprintf('funm(diag(2,3), @exp): F(1,1)=%.10f  F(2,2)=%.10f  (expect 7.3890560989 20.0855369232)\n', ...
        F(1,1), F(2,2));

% Non-symmetric, distinct real eigenvalues.
F = funm([1 2; 3 4], @exp);
fprintf('funm([1 2;3 4], @exp): F(1,1)=%.8f  F(1,2)=%.8f  (expect 51.96895620 74.73656458)\n', ...
        F(1,1), F(1,2));

% Trig matrix functions.
Fs = funm([1 2; 3 4], @sin);
Fc = funm([1 2; 3 4], @cos);
fprintf('funm([1 2;3 4], @sin): F(1,1)=%.8f  (expect -0.46558149)\n', Fs(1,1));
fprintf('funm([1 2;3 4], @cos): F(1,1)=%.8f  (expect  0.85542317)\n', Fc(1,1));

% Symmetric @sqrt agrees with sqrtm.
F = funm([2 1; 1 2], @sqrt);
G = sqrtm([2 1; 1 2]);
fprintf('funm(sym, @sqrt) vs sqrtm: maxdiff=%.3e  (expect ~0)\n', max(max(abs(F - G))));

% @exp agrees with expm.
F = funm([1 2; 3 4], @exp);
G = expm([1 2; 3 4]);
fprintf('funm(A, @exp) vs expm:     maxdiff=%.3e  (expect ~0)\n', max(max(abs(F - G))));
