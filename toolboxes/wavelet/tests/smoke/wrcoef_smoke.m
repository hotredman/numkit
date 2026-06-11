clear

import compat.*

v10 = 1:16;
[c, l] = wavedec(v10, 3, 'haar');

a3 = wrcoef('a', c, l, 'haar', 3);
fprintf('a3 (level=3 approx): '); disp(a3');
fprintf('  expect [4.5×8, 12.5×8]\n');

a0 = wrcoef('a', c, l, 'haar', 0);
fprintf('a0 vs original: max diff = %.4e\n', max(abs(a0 - v10')));

recon = a3 + wrcoef('d', c, l, 'haar', 1) + wrcoef('d', c, l, 'haar', 2) ...
       + wrcoef('d', c, l, 'haar', 3);
fprintf('a3 + sum(d_i) vs a0: max diff = %.4e\n', max(abs(recon - a0)));
