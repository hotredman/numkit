clear

import compat.*

% imsmooth — only Gaussian mode supported.

I = zeros(5, 5);
I(3, 3) = 1;

fprintf('--- imsmooth(impulse, "Gaussian", 0.5) ---\n');
J = imsmooth(I, 'Gaussian', 0.5);
fprintf('size %s, sum = %.4f (expect ~1)\n', mat2str(size(J)), sum(J(:)));
fprintf('center  = %.4f\n', J(3, 3));
fprintf('neighbour (3,4) = %.6f\n', J(3, 4));

fprintf('\n--- imsmooth(I, sigma) shorthand (scalar second arg = sigma) ---\n');
K = imsmooth(I, 0.5);
fprintf('match Gaussian-form? %d\n', isequal(J, K));

fprintf('\n--- default sigma = 0.5 ---\n');
M = imsmooth(I);
fprintf('match? %d\n', isequal(J, M));

fprintf('\n--- larger sigma ---\n');
L = imsmooth(I, 'Gaussian', 1.0);
fprintf('center = %.4f, sigma=1 spreads more (smaller center)\n', L(3, 3));
