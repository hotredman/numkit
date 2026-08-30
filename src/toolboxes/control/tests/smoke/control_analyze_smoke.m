clear

% --- dcgain on G(s) = 5/(s+1) — DC gain = 5 ---
G = tf(5, [1 1]);
fprintf('--- dcgain(5/(s+1)) ---\n');
fprintf('  dcgain = %.4f (expect 5.0)\n\n', real(dcgain(G)));

% --- dcgain on G(s) = (s+2)/(s^2+3s+2) — H(0) = 2/2 = 1 ---
H = tf([1 2], [1 3 2]);
fprintf('--- dcgain((s+2)/(s^2+3s+2)) ---\n');
fprintf('  dcgain = %.4f (expect 1.0)\n\n', real(dcgain(H)));

% --- dcgain discrete: G(z) = 1/(z-0.5), Ts=0.1 — H(z=1) = 1/0.5 = 2 ---
Gd = tf(1, [1 -0.5], 0.1);
fprintf('--- dcgain(1/(z-0.5), Ts=0.1) ---\n');
fprintf('  dcgain = %.4f (expect 2.0)\n\n', real(dcgain(Gd)));

% --- margin on a textbook 1/(s(s+1)(s+2)) loop ---
%   At Wcp ≈ 0.45, |H|=1, and phase ≈ -180° at Wcg ≈ sqrt(2) ≈ 1.41.
%   At Wcg, |H| = 1/(sqrt(2)·sqrt(3)·sqrt(2)) = 1/(2·sqrt(3)·sqrt(2)/sqrt(2))
%   Closed form: at ω where phase=-180, |H| = 1/(2·3) = 1/6 → Gm = 6.
L = tf(1, [1 3 2 0]);   % 1/(s(s+1)(s+2)) = 1/(s^3 + 3s^2 + 2s)
[Gm, Pm, Wcg, Wcp] = margin(L);
fprintf('--- margin(1/(s(s+1)(s+2))) ---\n');
fprintf('  Gm   = %.4f (expect ≈ 6.0, linear; 15.56 dB)\n', Gm);
fprintf('  Pm   = %.4f deg (expect ≈ 53.4°)\n', Pm);
fprintf('  Wcg  = %.4f rad/s (expect ≈ 1.414, sqrt(2))\n', Wcg);
fprintf('  Wcp  = %.4f rad/s (expect ≈ 0.446)\n\n', Wcp);

% --- stepinfo on G(s) = 1/(s+1) — first-order ---
G1 = tf(1, [1 1]);
S = stepinfo(G1);
fprintf('--- stepinfo(1/(s+1)) ---\n');
fprintf('  RiseTime     = %.4f (expect ≈ 2.197 = ln(9))\n', S.RiseTime);
fprintf('  SettlingTime = %.4f (expect ≈ 3.91 = -ln(0.02))\n', S.SettlingTime);
fprintf('  Overshoot    = %.4f%% (expect 0)\n', S.Overshoot);
fprintf('  Peak         = %.4f (expect ≈ 1.0)\n\n', S.Peak);

% --- stepinfo on underdamped wn=5, ζ=0.3 ---
% Theoretical overshoot = 100 * exp(-zeta*pi/sqrt(1-zeta^2)) = 100 * exp(-0.3π/sqrt(0.91)) ≈ 37.2%
G3 = tf(25, [1 3 25]);
S3 = stepinfo(G3);
fprintf('--- stepinfo(25/(s^2+3s+25), ζ=0.3, wn=5) ---\n');
fprintf('  Overshoot    = %.2f%% (expect ≈ 37.2%%)\n', S3.Overshoot);
fprintf('  PeakTime     = %.4f (expect ≈ pi/(wn*sqrt(1-zeta^2)) = 0.659)\n', S3.PeakTime);
fprintf('  Peak         = %.4f\n', S3.Peak);
