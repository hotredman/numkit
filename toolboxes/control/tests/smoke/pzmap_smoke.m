clear

import compat.*

% --- pzmap on G(s) = (s+2)/((s+1)(s+3)) ---
G = zpk([-2], [-1; -3], 1);
[p, z] = pzmap(G);
fprintf('--- pzmap(zpk([-2], [-1; -3], 1)) ---\n');
fprintf('  poles = '); disp(p');
fprintf('  zeros = '); disp(z');
fprintf('  expect poles = [-1 -3], zeros = [-2]\n\n');

% Same via tf:
G_tf = tf([1 2], conv([1 1], [1 3]));   % (s+2)/((s+1)(s+3))
[p2, z2] = pzmap(G_tf);
fprintf('--- pzmap on tf form (same plant) ---\n');
fprintf('  poles = '); disp(p2');
fprintf('  zeros = '); disp(z2');

% --- isstatic ---
S0 = tf(7, 1);          % pure gain (no s in denominator)
S1 = tf(1, [1 1]);      % 1/(s+1) — has dynamics
fprintf('--- isstatic ---\n');
fprintf('  isstatic(tf(7, 1))   = %d (expect 1)\n', isstatic(S0));
fprintf('  isstatic(tf(1, [1 1])) = %d (expect 0)\n\n', isstatic(S1));

% --- tzero on SISO == zero ---
G3 = tf([1 -2], [1 3 2]);   % (s-2)/(s^2+3s+2)
tz = tzero(G3);
zz = zero(G3);
fprintf('--- tzero == zero on SISO ---\n');
fprintf('  tzero = '); disp(tz');
fprintf('  zero  = '); disp(zz');
fprintf('  match diff = %.6e (expect 0)\n', max(abs(tz - zz)));
