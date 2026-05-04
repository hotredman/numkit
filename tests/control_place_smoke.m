import compat.*

% --- Trivial scalar: A=0, B=1, want pole at -3 ⇒ K = 3 ---
K = acker(0, 1, [-3]);
fprintf('--- acker(0, 1, [-3]) ---\n');
fprintf('  K = %.4f (expect 3.0)\n\n', K);

% --- Double integrator A=[0 1; 0 0], B=[0;1]; place at -2±j ---
A = [0 1; 0 0];
B = [0; 1];
% Desired poles: -2 ± j → φ(s) = s² + 4s + 5
% Closed-loop A_cl = A - B*K must have char poly s² + 4s + 5.
% Plant char poly is s². We need K = [k1 k2] with
% A_cl = [0 1; -k1 -k2], char poly s² + k2·s + k1 → k1=5, k2=4.
K = acker(A, B, [-2+1i; -2-1i]);
fprintf('--- acker(2-state integrator, [-2±j]) ---\n');
disp(K);
fprintf('  expect [5 4]\n\n');

% --- Verify closed-loop poles match desired ---
A_cl = A - B*K;
fprintf('  A_cl =\n'); disp(A_cl);
% Closed-loop char poly via numkit poly()-roots() round-trip:
charP = [1, -A_cl(1,1) - A_cl(2,2), A_cl(1,1)*A_cl(2,2) - A_cl(1,2)*A_cl(2,1)];
fprintf('  char poly = '); disp(charP);
fprintf('  expect [1 4 5]\n\n');

% --- 3rd-order plant — also stable ---
A3 = [0 1 0; 0 0 1; 0 0 0];   % triple integrator
B3 = [0; 0; 1];
% Desired: -1, -2, -3 → φ(s) = (s+1)(s+2)(s+3) = s³+6s²+11s+6
K3 = acker(A3, B3, [-1; -2; -3]);
fprintf('--- acker(triple integrator, [-1, -2, -3]) ---\n');
disp(K3);
fprintf('  expect [6 11 6]\n\n');

% Verify closed-loop poles:
A3_cl = A3 - B3*K3;
% Char poly of A3_cl must equal [1 6 11 6]; check by Cayley-Hamilton:
% pole(ss(A3_cl, B3, [1 0 0], 0)) should give roots ≈ -1, -2, -3.
S_cl = ss(A3_cl, B3, [1 0 0], 0);
poles_cl = pole(S_cl);
fprintf('  poles(A_cl) = '); disp(poles_cl');
fprintf('  expect [-1; -2; -3] (in some order)\n\n');

% --- place is alias for acker on SISO ---
K_p = place(A, B, [-2+1i; -2-1i]);
fprintf('--- place identical to acker? ---\n');
fprintf('  max|K_acker - K_place| = %.2e\n', max(abs(K_p - acker(A, B, [-2+1i; -2-1i]))));

% --- Uncontrollable: should error ---
try
    K_uc = acker(eye(2), [1; 0], [-1; -2]);
    fprintf('\n--- acker uncontrollable: no error (BUG?)\n');
catch err
    fprintf('\n--- acker correctly errors on uncontrollable: %s\n', err.message);
end
