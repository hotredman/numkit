clear
import compat.*

fprintf('=== rref / rcond / planerot ===\n');

% --- rref ---
fprintf('\n[rref]\n');
A = [1 2 3; 4 5 6; 7 8 10];
fprintf('rref full-rank 3x3:\n'); disp(rref(A));
fprintf('  expect identity 3x3\n');

A2 = [1 2 3; 2 4 6; 3 6 9];
[R2, jb2] = rref(A2);
fprintf('rref rank-1:\n'); disp(R2);
fprintf('  jb = '); fprintf('%d ', jb2); fprintf(' (expect [1])\n');

A3 = [1 2 0 1; 0 0 1 2; 1 2 1 3];
[R3, jb3] = rref(A3);
fprintf('rref 3x4 rank-2:\n'); disp(R3);
fprintf('  jb = '); fprintf('%d ', jb3); fprintf(' (expect [1 3])\n');

fprintf('rref 2x4 wide:\n'); disp(rref([1 2 3 4; 5 6 7 8]));

% --- rcond ---
fprintf('\n[rcond]\n');
fprintf('  rcond(eye(3))    = %.10g (expect 1)\n', rcond(eye(3)));
fprintf('  rcond([2 0;0 3]) = %.10g (expect 0.6667)\n', rcond([2 0; 0 3]));
fprintf('  rcond([1 2;3 4]) = %.10g (expect 0.04762)\n', rcond([1 2; 3 4]));
fprintf('  rcond(hilb(4))   = %.5e (expect ~3.5e-5)\n', rcond(hilb(4)));
fprintf('  rcond([1 2;2 4]) = %.10g (singular -> 0)\n', rcond([1 2; 2 4]));

% --- planerot ---
fprintf('\n[planerot]\n');
[G, y] = planerot([3; 4]);
fprintf('  planerot([3;4]):\n'); disp(G);
fprintf('  y = '); fprintf('%g ', y); fprintf(' (expect [5 0])\n');

[G2, y2] = planerot([1; 0]);
fprintf('  planerot([1;0]) G:\n'); disp(G2);
fprintf('  y = '); fprintf('%g ', y2); fprintf('\n');

[G3, y3] = planerot([0; 0]);
fprintf('  planerot([0;0]) G:\n'); disp(G3);
fprintf('  y = '); fprintf('%g ', y3); fprintf('\n');

[G4, y4] = planerot([-3; 4]);
fprintf('  planerot([-3;4]):\n'); disp(G4);
fprintf('  y = '); fprintf('%g ', y4); fprintf(' (expect [5 0])\n');

% Verify G*[x;y] = [r;0]
v = [3; 4];
[G, y] = planerot(v);
res = G*v - y;
fprintf('  residual ||G*v - y|| = %.2e\n', norm(res));
