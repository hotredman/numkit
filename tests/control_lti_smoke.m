import compat.*

% --- tf constructor ---
G = tf([1 2], [1 3 5]);
fprintf('--- tf([1 2], [1 3 5]) ---\n');
fprintf('  kind = %s, Ts = %g\n', G.kind, G.Ts);
fprintf('  num = '); disp(G.num);
fprintf('  den = '); disp(G.den);
fprintf('\n');

% --- zpk constructor ---
H = zpk([-1; -2], [-3; -4; -5], 2.0);
fprintf('--- zpk([-1 -2], [-3 -4 -5], 2.0) ---\n');
fprintf('  kind = %s, k = %g\n', H.kind, H.k);
fprintf('  z = '); disp(H.z);
fprintf('  p = '); disp(H.p);
fprintf('\n');

% --- ss constructor ---
A = [-2 1; 0 -3];
B = [0; 1];
C = [1 0];
D = 0;
S = ss(A, B, C, D);
fprintf('--- ss(A, B, C, D) ---\n');
fprintf('  kind = %s, A = \n', S.kind); disp(S.A);
fprintf('\n');

% --- tf2zp / zp2tf round-trip ---
[z, p, k] = tf2zp([1 0 -2], [1 0 1]);
fprintf('--- tf2zp([1 0 -2], [1 0 1]) ---\n');
fprintf('  z = '); disp(z);
fprintf('  p = '); disp(p);
fprintf('  k = %g (expect 1)\n\n', k);

% Round-trip:
[num, den] = zp2tf(z, p, k);
fprintf('--- zp2tf round-trip ---\n');
fprintf('  num = '); disp(num);
fprintf('  den = '); disp(den);
fprintf('  expect: num = [1 0 -2], den = [1 0 1]\n\n');

% --- tf2ss / ss2tf round-trip on G(s) = (s+2)/(s^2+3s+5) ---
[A, B, C, D] = tf2ss([1 2], [1 3 5]);
fprintf('--- tf2ss([1 2], [1 3 5]) ---\n');
fprintf('  A =\n'); disp(A);
fprintf('  B = '); disp(B');
fprintf('  C = '); disp(C);
fprintf('  D = %g\n\n', D);

[num, den] = ss2tf(A, B, C, D);
fprintf('--- ss2tf back ---\n');
fprintf('  num = '); disp(num);
fprintf('  den = '); disp(den);
fprintf('  expect: num = [0 1 2], den = [1 3 5]\n\n');

% --- Higher-order roundtrip on (s^3 + 2s^2 + 3s + 4)/(s^4 + 5s^3 + 6s^2 + 7s + 8) ---
num = [1 2 3 4];
den = [1 5 6 7 8];
[A, B, C, D] = tf2ss(num, den);
[num2, den2] = ss2tf(A, B, C, D);
fprintf('--- 4th-order tf2ss-ss2tf round-trip ---\n');
fprintf('  num err = %.6e\n', max(abs(num2(2:end) - num)));
fprintf('  den err = %.6e\n', max(abs(den2 - den)));
