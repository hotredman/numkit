import compat.*

% --- step on G(s) = 1/(s+1): final value should approach 1 ---
G = tf(1, [1 1]);
[y, t] = step(G, 6);
fprintf('--- step(1/(s+1), 6) ---\n');
fprintf('  y(1)   = %.4f (expect 0.0)\n', y(1));
fprintf('  y(end) = %.4f (expect ~ 1.0)\n', y(end));
fprintf('  numel(t) = %d\n\n', numel(t));

% Settling check at 5τ ≈ 5s: 1 - exp(-5) ≈ 0.9933.
half = round(0.5 * numel(t));
expected_at_half = 1 - exp(-t(half));
fprintf('  y(t≈%.2f) = %.4f (expect ~ %.4f = 1-exp(-%.2f))\n\n', ...
    t(half), y(half), expected_at_half, t(half));

% --- impulse on G(s) = 1/(s+1): y(t) = exp(-t) ---
[y, t] = impulse(G, 5);
fprintf('--- impulse(1/(s+1), 5) ---\n');
fprintf('  y(1) = %.4f (expect 1.0 — ZOH on first sample lowers it slightly)\n', y(1));
fprintf('  y(end) = %.4e (expect ~ exp(-5) = %.4e)\n', y(end), exp(-5));

% --- 2nd-order: G = 1/(s^2+2s+1), step should converge to 1 ---
G2 = tf(1, [1 2 1]);
[y2, t2] = step(G2, 8);
fprintf('\n--- step(1/(s^2+2s+1), 8) ---\n');
fprintf('  y2(end) = %.4f (expect ~ 1.0)\n', y2(end));

% Damped oscillator: G = 25/(s^2+s+25), zeta = 0.1, wn = 5
G3 = tf(25, [1 1 25]);
[y3, t3] = step(G3, 6);
fprintf('\n--- step(underdamped wn=5,ζ=0.1, 6) ---\n');
fprintf('  max(y3) = %.4f (expect overshoot > 1.5)\n', max(y3));
fprintf('  y3(end) = %.4f (expect ~ 1.0)\n', y3(end));

% --- lsim with sine input on G(s) = 1/(s+1) ---
t = 0:0.05:5;
u = sin(2*pi*0.5*t);
y = lsim(G, u, t);
fprintf('\n--- lsim sine into 1/(s+1) ---\n');
fprintf('  numel(y) = %d\n', numel(y));
fprintf('  max(|y|) ≈ %.3f, max(|u|) = 1\n', max(abs(y)));
fprintf('  expect: |y| < 1 (lowpass attenuates)\n');

% --- ss-form input ---
A = [-2, 0; 0, -3];
B = [1; 1];
C = [1, 1];
D = 0;
S = ss(A, B, C, D);
[y, t] = step(S, 4);
fprintf('\n--- step(ss(A,B,C,D), 4) ---\n');
fprintf('  y(end) = %.4f (expect ~ 1/2 + 1/3 = 0.8333)\n', y(end));

% --- Discrete: x[k+1] = 0.5 x[k] + u, y = x ---
Gd = ss(0.5, 1, 1, 0, 0.1);
[y, t] = step(Gd, 2);
fprintf('\n--- step(discrete, Ts=0.1) ---\n');
fprintf('  y(end) = %.4f (expect ~ 1/(1-0.5) = 2.0)\n', y(end));
