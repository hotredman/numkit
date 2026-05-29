clear
import compat.*

% eig 'vector'/'matrix' options + generalized eig(A,B).
A = [2 0 0; 0 3 0; 0 0 5];

% Default 1-output is already a column vector.
fprintf('default  : %s\n', mat2str(eig(A).'));            % expect [2 3 5]

% 'vector' — explicit column of eigenvalues.
ev = eig(A, 'vector');
fprintf('vector   : %s (col? %d)\n', mat2str(ev.'), iscolumn(ev));  % [2 3 5], col 1

% 'matrix' — diagonal matrix even with one output.
D = eig(A, 'matrix');
fprintf('matrix   : %dx%d diag %s\n', size(D,1), size(D,2), mat2str(diag(D).')); % 3x3 [2 3 5]

% Generalized eig(A,B): eigenvalues of B\A.
B = [1 0 0; 0 2 0; 0 0 1];
fprintf('gen diag : %s\n', mat2str(sort(eig(A, B)).'));   % expect [1.5 2 5]

% Symmetric-definite pair → real eigenvalues.
As = [4 1; 1 3]; Bs = [2 0; 0 1];
g = eig(As, Bs);
fprintf('gen sym  : %s (imag %.2e)\n', mat2str(sort(real(g)).'), max(abs(imag(g)))); % [1.6339.. 3.3660..]

% [V,D] = eig(A,B) reconstructs: A*V = B*V*D.
[V, Dg] = eig(As, Bs);
fprintf('recon err: %.2e\n', max(max(abs(As*V - Bs*V*Dg))));  % expect ~0
