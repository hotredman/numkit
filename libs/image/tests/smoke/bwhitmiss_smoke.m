clear

import compat.*

% bwhitmiss — binary hit-or-miss morphological transform.

% --- Octave-source reference ---
bw1 = repmat([0 1 0 1 1], [3 1]);
bw2 = repmat([0 1 0 0 0], [3 1]);

fprintf('--- bwhitmiss(bw, [1;0;1], [1 0 1]) ---\n');
J = bwhitmiss(bw1, [1;0;1], [1 0 1]);
disp(double(J));
fprintf('  expect: hits column 2 only (rows of "0;1;0" with no horiz neighbours)\n\n');

% --- using interval form ---
fprintf('--- bwhitmiss(bw, interval) ---\n');
intv = [0 1 0; -1 0 -1; 0 1 0];
J2 = bwhitmiss(bw1, intv);
disp(double(J2));
fprintf('  expect: same shape as bw1; 1s where (0,1,0) vert neighbours hit\n');

% --- check equivalence vs imerode formula ---
fprintf('\n--- bwhitmiss == imerode(bw, se1) & imerode(~bw, se2) ---\n');
se1 = [1;0;1];
se2 = [1 0 1];
J3 = imerode(bw1, se1) & imerode(~bw1, se2);
fprintf('match = %d (expect 1)\n', isequal(J, J3));
