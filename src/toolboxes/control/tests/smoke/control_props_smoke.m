clear

% Stable continuous: G(s) = 1/(s+1), pole at s = -1
G = tf([1], [1 1]);
fprintf('--- G = 1/(s+1) ---\n');
fprintf('  isct  = %d (expect 1)\n', isct(G));
fprintf('  isdt  = %d (expect 0)\n', isdt(G));
fprintf('  issiso = %d (expect 1)\n', issiso(G));
fprintf('  isproper = %d (expect 1)\n', isproper(G));
fprintf('  order  = %d (expect 1)\n', order(G));
fprintf('  pole   = '); disp(pole(G)');
fprintf('  zero   = '); disp(zero(G)');
fprintf('\n');

% Unstable: G(s) = 1/(s-1)
H = tf([1], [1 -1]);
fprintf('--- H = 1/(s-1) ---\n');
fprintf('  pole   = '); disp(pole(H)');

% --- pole / zero / order on zpk ---
K = zpk([-2], [-3, -4], 5);
fprintf('--- K = 5(s+2)/((s+3)(s+4)) ---\n');
fprintf('  order  = %d (expect 2)\n', order(K));
fprintf('  pole   = '); disp(pole(K));
fprintf('  zero   = '); disp(zero(K));

% --- ss form: poles via Faddeev ---
A = [-2 1; 0 -3];
B = [0; 1];
C = [1 0];
D = 0;
S = ss(A, B, C, D);
fprintf('--- ss with eigvals -2, -3 ---\n');
fprintf('  pole   = '); disp(pole(S)');
fprintf('  order  = %d (expect 2)\n', order(S));
fprintf('  issiso = %d (expect 1)\n', issiso(S));

% --- damp on continuous 2nd order: G = wn^2 / (s^2 + 2*zeta*wn*s + wn^2)
wn0 = 5; zeta0 = 0.3;
G2 = tf(wn0^2, [1, 2*zeta0*wn0, wn0^2]);
[wn, zeta_out, p] = damp(G2);
fprintf('\n--- damp on wn=5, zeta=0.3 ---\n');
fprintf('  wn   = '); disp(wn');
fprintf('  zeta = '); disp(zeta_out');
fprintf('  expect: wn = [5; 5], zeta = [0.3; 0.3]\n');

% --- Discrete stability: poles inside unit circle ---
Gd = tf([1], [1, -0.5], 0.1);
fprintf('\n--- Gd = 1/(z-0.5), Ts=0.1 ---\n');
fprintf('  isct  = %d (expect 0)\n', isct(Gd));
fprintf('  isdt  = %d (expect 1)\n', isdt(Gd));
fprintf('  control.props.isstable(Gd) = %d (expect 1)\n', control.props.isstable(Gd));
