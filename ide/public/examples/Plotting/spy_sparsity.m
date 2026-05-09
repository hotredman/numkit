% spy — Sparsity pattern of a matrix
% Marker at every non-zero entry; row 1 sits at the top (axis ij is
% set automatically). Useful to visualise band structure / fill-in
% in numerical methods.
import compat.*;

clear
close all

% Tridiagonal-with-corner matrix (a 1-D Laplacian + periodic links).
n = 30;
M = zeros(n, n);
for i = 1:n
    M(i, i) = 2;
    if i > 1; M(i, i-1) = -1; end
    if i < n; M(i, i+1) = -1; end
end
% Periodic corner couplings.
M(1, n) = -1;
M(n, 1) = -1;

figure
spy(M)
title('spy — Periodic 1-D Laplacian')
