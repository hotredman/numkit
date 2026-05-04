import compat.*

% --- c2d ZOH on G(s) = 1/(s+2) at Ts=0.1 ---
% Analytic: A_d = exp(-2*0.1) = exp(-0.2) ≈ 0.8187
%           B_d = (1 - exp(-0.2))/2 ≈ 0.0906
G = tf(1, [1 2]);
Gd = c2d(G, 0.1);  % default 'zoh'
fprintf('--- c2d(1/(s+2), Ts=0.1) ZOH ---\n');
fprintf('  Ts = %g (expect 0.1)\n', Gd.Ts);
fprintf('  num = '); disp(Gd.num);
fprintf('  den = '); disp(Gd.den);
fprintf('  expect num ≈ [0 0.0906], den ≈ [1 -0.8187]\n\n');

% --- DC gains preserved across c2d ZOH ---
fprintf('  dcgain(continuous) = %.4f (expect 0.5)\n', real(dcgain(G)));
fprintf('  dcgain(discrete)   = %.4f (expect 0.5)\n\n', real(dcgain(Gd)));

% --- c2d Tustin on integrator G(s) = 1/s ---
% Tustin of 1/s with Ts=0.5: y[k+1] = y[k] + (Ts/2)*(u[k]+u[k+1])
%   ⇒ G_d(z) = (Ts/2)·(z+1)/(z-1)
Gint = tf(1, [1 0]);
GintD = c2d(Gint, 0.5, 'tustin');
fprintf('--- c2d(1/s, Ts=0.5) Tustin ---\n');
fprintf('  num = '); disp(GintD.num);
fprintf('  den = '); disp(GintD.den);
fprintf('  expect num ≈ [0.25 0.25], den ≈ [1 -1]\n\n');

% --- Round trip: continuous → discrete ZOH → step responses align at sample times ---
[yC, tC] = step(G, 1.0);
[yD, tD] = step(Gd, 1.0);
% Sample tC at the same instants as tD:
[~, kC] = min(abs(tC - 0.5));
[~, kD] = min(abs(tD - 0.5));
fprintf('--- step alignment at t=0.5 ---\n');
fprintf('  continuous y(0.5) = %.4f\n', yC(kC));
fprintf('  discrete   y(0.5) = %.4f\n', yD(kD));
fprintf('  expect: very close (ZOH equivalence)\n\n');

% --- d2c Tustin on previous discrete result ---
GdT = c2d(G, 0.5, 'tustin');
GcT = d2c(GdT, 'tustin');
fprintf('--- d2c(c2d(G, Ts=0.5, tustin), tustin) round-trip ---\n');
fprintf('  num = '); disp(GcT.num);
fprintf('  den = '); disp(GcT.den);
fprintf('  expect: ≈ [1] / [1 2] (original G)\n\n');

% --- ss form preservation ---
A = [-3 1; 0 -2];
B = [1; 1];
C = [1 0];
D = 0;
S = ss(A, B, C, D);
Sd = c2d(S, 0.1);
fprintf('--- c2d(ss form, Ts=0.1) ZOH ---\n');
fprintf('  kind = %s, Ts = %g\n', Sd.kind, Sd.Ts);
fprintf('  A_d (expect contractive, eigvals exp(-0.3)≈0.7408, exp(-0.2)≈0.8187):\n');
disp(Sd.A);
