clear

import compat.*

% medfilt3 — 3-D median filter, default 3x3x3, symmetric pad.

fprintf('--- ones(3,3,3) → all 1 ---\n');
V = ones(3,3,3);
disp(any(medfilt3(V)(:) ~= 1));
fprintf('  expect 0 (none differ)\n\n');

fprintf('--- single-spike, 5x5x5 ---\n');
V = zeros(5,5,5);
V(3,3,3) = 100;
J = medfilt3(V);
fprintf('center pixel: %.4f  (expect 0 — single spike removed by median)\n', J(3,3,3));
fprintf('max value: %.4f  (expect 0 — spike is the only nonzero, drowned)\n', max(J(:)));

fprintf('\n--- non-cube filter [3 1 1] (vertical only) ---\n');
V = zeros(7,3,3);
V(4,2,2) = 1;
J = medfilt3(V, [3 1 1]);
fprintf('size: %s\n', mat2str(size(J)));
fprintf('center: %.4f, max: %.4f\n', J(4,2,2), max(J(:)));
