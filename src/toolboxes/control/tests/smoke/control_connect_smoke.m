clear

% --- series: 1/(s+1) cascaded with 1/(s+2) → 1/((s+1)(s+2)) = 1/(s^2+3s+2) ---
G1 = tf(1, [1 1]);
G2 = tf(1, [1 2]);
S = series(G1, G2);
fprintf('--- series(1/(s+1), 1/(s+2)) ---\n');
fprintf('  num = '); disp(S.num);
fprintf('  den = '); disp(S.den);
fprintf('  expect num = [1], den = [1 3 2]\n\n');

% --- parallel: 1/(s+1) + 1/(s+2) = (2s+3)/((s+1)(s+2)) ---
P = parallel(G1, G2);
fprintf('--- parallel(1/(s+1), 1/(s+2)) ---\n');
fprintf('  num = '); disp(P.num);
fprintf('  den = '); disp(P.den);
fprintf('  expect num = [2 3], den = [1 3 2]\n\n');

% --- feedback default (-1, negative): G/(1+GH) ---
%   G = 1/s, H = 1 → T = 1/(s+1)
G = tf(1, [1 0]);
H = tf(1, [1]);
T = feedback(G, H);
fprintf('--- feedback(1/s, 1) ---\n');
fprintf('  num = '); disp(T.num);
fprintf('  den = '); disp(T.den);
fprintf('  expect num = [1], den = [1 1]\n\n');

% --- feedback with sign = +1 (positive): G/(1-GH)
%   G = 1/(s-1), H = 1, sign=-1 (negative) → T = 1/((s-1) + 1) = 1/s -- pole at 0
T2 = feedback(tf(1, [1 -1]), tf(1, [1]), -1);
fprintf('--- feedback(1/(s-1), 1, -1 negative) ---\n');
fprintf('  num = '); disp(T2.num);
fprintf('  den = '); disp(T2.den);
fprintf('  expect num = [1], den = [1 0]\n\n');

% --- Zpk → tf path for series ---
K = zpk([], [-1, -2], 1);    % equivalent to G1*G2
S2 = series(zpk([], [-1], 1), zpk([], [-2], 1));
fprintf('--- series(zpk(-1), zpk(-2)) round-trip via tf ---\n');
fprintf('  num = '); disp(S2.num);
fprintf('  den = '); disp(S2.den);

% --- ss inputs ---
G_ss = ss(-1, 1, 1, 0);     % 1/(s+1) in SS
H_ss = ss(-2, 1, 1, 0);     % 1/(s+2)
S3 = series(G_ss, H_ss);
fprintf('--- series(ss form) ---\n');
fprintf('  num = '); disp(S3.num);
fprintf('  den = '); disp(S3.den);
fprintf('  expect num = [1], den = [1 3 2]\n\n');

% --- Closed-loop pole verification ---
T3 = feedback(tf([1], [1 2 1]), tf([1], [1]));
fprintf('--- feedback(1/(s^2+2s+1), 1) ---\n');
fprintf('  num = '); disp(T3.num);
fprintf('  den = '); disp(T3.den);
fprintf('  poles = '); disp(pole(T3)');
fprintf('  expect den = [1 2 2], poles ~ -1+/-i\n');

% --- NUMERIC static gain (a scalar is the system K/1) ---
fprintf('\n--- numeric static gain ---\n');
fprintf('  feedback(1/(s+1), 1) dcgain = %.4f  (expect 0.5)\n', ...
        dcgain(feedback(tf(1, [1 1]), 1)));
fprintf('  feedback(2, 1/(s+1)) dcgain = %.4f  (expect 0.6667)\n', ...
        dcgain(feedback(2, tf(1, [1 1]))));
fprintf('  series(2/(s+1), 3)   dcgain = %.4f  (expect 6)\n', ...
        dcgain(series(tf(2, [1 1]), 3)));
fprintf('  parallel(2/(s+1), 3) dcgain = %.4f  (expect 5)\n', ...
        dcgain(parallel(tf(2, [1 1]), 3)));
