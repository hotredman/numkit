clear

import compat.*

% --- Double integrator: A=[0 1; 0 0], B=[0;1], C=[1 0] is fully observable+controllable ---
A = [0 1; 0 0];
B = [0; 1];
C = [1 0];
Co = ctrb(A, B);
Ob = obsv(A, C);
fprintf('--- ctrb on double-integrator ---\n');
disp(Co);
fprintf('  expect [0 1; 1 0], rank 2\n\n');
fprintf('--- obsv on double-integrator ---\n');
disp(Ob);
fprintf('  expect [1 0; 0 1], rank 2\n\n');

% --- Uncontrollable: A=I_2, B=[1;0]. ctrb = [B AB] = [B B] rank 1 ---
Au = eye(2);
Bu = [1; 0];
Cu_uncontrol = ctrb(Au, Bu);
fprintf('--- ctrb on (I, [1;0]) — uncontrollable ---\n');
disp(Cu_uncontrol);
fprintf('  expect [1 1; 0 0] (block 1 = B, block 2 = I·B = B again)\n\n');

% --- ctrb shape on multi-input system: A 2x2, B 2x3 → ctrb is 2x6 ---
A2 = [-1 0; 0 -2];
B2 = [1 0 1; 0 1 1];
Cm = ctrb(A2, B2);
fprintf('--- ctrb size with 2-state, 3-input ---\n');
fprintf('  size(Co) = %dx%d (expect 2 x 6)\n\n', size(Cm,1), size(Cm,2));

% --- ctrb / obsv on tf via 1-arg ---
G = tf(1, [1 3 2]);    % 1/(s^2 + 3s + 2), controllable canonical form
Co1 = ctrb(G);
Ob1 = obsv(G);
fprintf('--- ctrb(tf) ---\n');
disp(Co1);
fprintf('  size = %dx%d\n\n', size(Co1,1), size(Co1,2));
fprintf('--- obsv(tf) ---\n');
disp(Ob1);
fprintf('  size = %dx%d\n\n', size(Ob1,1), size(Ob1,2));

% --- ctrb on ss form ---
A3 = [-2 1 0; 0 -3 1; 0 0 -4];   % triangular, distinct eigvals
B3 = [1; 0; 0];                  % only first state directly excited
S = ss(A3, B3, [0 0 1], 0);
Co3 = ctrb(S);
fprintf('--- ctrb(ss) for 3x3 triangular A ---\n');
disp(Co3);
fprintf('  rank should be 0 (B is in left invariant subspace), but expect rank 1\n');
fprintf('  (only state 1 is excited; without coupling, others stay at 0)\n');
