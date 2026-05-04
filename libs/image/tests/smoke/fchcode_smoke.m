clear

import compat.*

% fchcode — Freeman 8-direction chain code for a closed boundary.

% Square traced clockwise from (1,1)→(1,3)→(3,3)→(3,1)→(1,1).
fprintf('--- 3x3 square (closed) ---\n');
b = [1 1; 1 2; 1 3; 2 3; 3 3; 3 2; 3 1; 2 1; 1 1];
fcc = fchcode(b);
fprintf('x0y0 = ['); fprintf('%g ', fcc.x0y0); fprintf(']\n');
fprintf('fcc  = ['); fprintf('%g ', fcc.fcc);  fprintf(']\n');
fprintf('  expect: x0y0=[1 1], fcc=[0 0 6 6 4 4 2 2]\n\n');

fprintf('diff = ['); fprintf('%g ', fcc.diff); fprintf(']\n');
fprintf('  expect: cyclic mod-8 first-difference\n');

% Auto-close: drop last point, fchcode appends it.
fprintf('\n--- auto-close on open boundary ---\n');
b2 = b(1:end-1, :);
fcc2 = fchcode(b2);
fprintf('match closed-form fcc? %d\n', isequal(fcc2.fcc, fcc.fcc));
