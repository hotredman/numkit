clear

% --- evalfr on G(s) = 1/(s+1) at s = j ---
G = tf(1, [1 1]);
H = evalfr(G, 1);     % H(j1) = 1/(1+j) = 0.5 - 0.5j, |H| = 1/sqrt(2)
fprintf('--- evalfr(1/(s+1), 1) ---\n');
fprintf('  H = %.4f + %.4fj  (expect 0.5 - 0.5j)\n', real(H), imag(H));
fprintf('  |H| = %.4f (expect 1/sqrt(2) = 0.7071)\n\n', abs(H));

% --- DC gain via evalfr at ω = 0 ---
H0 = evalfr(G, 0);
fprintf('--- evalfr(G, 0) DC gain ---\n');
fprintf('  H(0) = %.4f (expect 1.0)\n\n', real(H0));

% --- bode on G(s) = 1/(s+1) at corner frequency ω=1: |H|=1/sqrt(2), phase=-45° ---
[mag, phase, w] = bode(G, [0.1, 1, 10]);
fprintf('--- bode(1/(s+1), [0.1 1 10]) ---\n');
fprintf('  mag   = '); disp(mag');
fprintf('  phase = '); disp(phase');
fprintf('  expect mag ≈ [0.995, 0.7071, 0.0995], phase ≈ [-5.7, -45, -84.3] deg\n\n');

% --- 2nd-order resonant peak ---
G2 = tf(25, [1 1 25]);
[mag2, phase2, w2] = bode(G2, [4, 5, 6]);
fprintf('--- bode(25/(s^2+s+25), [4 5 6]) ---\n');
fprintf('  mag = '); disp(mag2');
fprintf('  expect mag at ω=5 ≈ 5 (peak near wn for ζ=0.1)\n');
fprintf('  ratio mag(2)/mag(1) = %.3f (expect > 1, resonance)\n\n', mag2(2)/mag2(1));

% --- nyquist: at ω=0, real=DC gain=1, imag=0 ---
[re, im, w] = nyquist(G, [0]);
fprintf('--- nyquist(1/(s+1), 0) ---\n');
fprintf('  re = %.4f, im = %.4f (expect 1, 0)\n\n', re(1), im(1));

% --- freqresp returns complex column ---
H = freqresp(G, [0, 1, 1000]);
fprintf('--- freqresp([0,1,1000]) ---\n');
fprintf('  |H(0)| = %.4f, |H(1)| = %.4f, |H(1000)| = %.6f\n', ...
    abs(H(1)), abs(H(2)), abs(H(3)));
fprintf('  expect: 1, 0.7071, ≈ 1e-3\n\n');

% --- Discrete: G(z) = 1/(z-0.5), Ts=0.1; |H(e^j0)| = 1/0.5 = 2 ---
Gd = tf(1, [1 -0.5], 0.1);
Hd = evalfr(Gd, 0);
fprintf('--- evalfr(1/(z-0.5), Ts=0.1, ω=0) ---\n');
fprintf('  H(z=1) = %.4f (expect 1/(1-0.5) = 2.0)\n', real(Hd));

% Default w grid via bode with no second arg
[mag, phase, w] = bode(G);
fprintf('\n--- bode(G) default grid ---\n');
fprintf('  numel(w) = %d, w(1) = %.4f, w(end) = %.4f\n', ...
    numel(w), w(1), w(end));
