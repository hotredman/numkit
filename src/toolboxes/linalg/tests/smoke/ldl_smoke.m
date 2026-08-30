clear

fprintf('=== ldl (block LDL'' factorization, v1 no pivoting) ===\n');

% Positive-definite case
A = [4 2 1; 2 5 3; 1 3 6];
fprintf('\n[ldl PD 3x3]:\n');
[L, D] = ldl(A);
fprintf('L:\n'); disp(L)
fprintf('  expect:\n    1.0000         0         0\n    0.5000    1.0000         0\n    0.2500    0.6250    1.0000\n');
fprintf('D:\n'); disp(D)
fprintf('  expect: diag([4 4 4.1875])\n');
res = norm(A - L*D*L');
fprintf('  ||A - L*D*L''|| = %.2e\n', res);

% Three-output form (P = identity in v1)
[L3, D3, P] = ldl(A);
fprintf('\n[ldl 3-out, P matrix]:\n');
fprintf('P:\n'); disp(P)
fprintf('  expect identity 3x3\n');

% Vector permutation
[Lv, Dv, p] = ldl(A, 'vector');
fprintf('\n[ldl 3-out, p vector]:\n');
fprintf('  p = '); fprintf('%d ', p); fprintf(' (expect 1 2 3)\n');

% Upper form
[Lu, Du] = ldl(A, 'upper');
fprintf('\n[ldl upper]:\n');
fprintf('Lu (unit upper):\n'); disp(Lu)
fprintf('  ||A - Lu''*Du*Lu|| = %.2e\n', norm(A - Lu'*Du*Lu));

% Indefinite case (works without pivoting if no zero pivots)
B = [2 -1; -1 -3];   % indefinite, but no pivoting needed
fprintf('\n[ldl indefinite 2x2]:\n');
[Lb, Db] = ldl(B);
fprintf('Lb:\n'); disp(Lb)
fprintf('Db:\n'); disp(Db)
fprintf('  ||B - Lb*Db*Lb''|| = %.2e\n', norm(B - Lb*Db*Lb'));

% 1-output form
fprintf('\n[ldl 1-out]:\n');
L1 = ldl([4 2; 2 5]);
fprintf('L1:\n'); disp(L1)
fprintf('  expect [1 0; 0.5 1]\n');

% Larger PD
fprintf('\n[ldl 5x5 PD]:\n');
P5 = magic(5); P5 = P5 + P5'; P5 = P5 + 50*eye(5);   % make SPD
[Lp, Dp] = ldl(P5);
fprintf('  ||P5 - Lp*Dp*Lp''|| = %.2e\n', norm(P5 - Lp*Dp*Lp'));
fprintf('  diag(Dp): '); fprintf('%.3f ', diag(Dp)); fprintf('\n');
