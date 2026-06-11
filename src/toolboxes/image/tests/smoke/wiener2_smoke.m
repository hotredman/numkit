clear

import compat.*

% wiener2 — adaptive Wiener noise reduction.

fprintf('--- wiener2(ones(5)) — Octave reference vector ---\n');
im0 = ones(5, 5);
[J, n] = wiener2(im0);
fprintf('size = %s, noise = %.5f (expect 0.1462)\n', mat2str(size(J)), n);
fprintf('J(1,1) = %.5f (expect 0.67111)\n', J(1, 1));
fprintf('J(1,2) = %.5f (expect 0.78074)\n', J(1, 2));
fprintf('J(2,2) = %.5f (expect 1.0)\n',     J(2, 2));

fprintf('\n--- explicit nhood [5 5] ---\n');
J5 = wiener2(im0, [5 5]);
fprintf('J5(1,1) = %.5f\n', J5(1, 1));
fprintf('J5(3,3) = %.5f\n', J5(3, 3));

fprintf('\n--- explicit noise scalar ---\n');
J05 = wiener2(im0, 0.5);
fprintf('J05(3,3) = %.4f (uniform → constant)\n', J05(3, 3));

fprintf('\n--- impulse + noise reduces ---\n');
A = zeros(20, 20);
A(10, 10) = 1;
[Aw, ~] = wiener2(A, [3 3]);
fprintf('center = %.4f (preserved), neighbour = %.4e (small)\n', ...
        Aw(10, 10), Aw(10, 11));
