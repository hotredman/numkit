%========================================================================
% SECTION F — CONTROL THEORY & ROBOTICS
%========================================================================

gainList = [2 0 0; 2 1 0; 2 1 0.1];
% plant = 1/(s^2+s+1); closed-loop num/den computed by hand:
%   den_cl = den*(den + numC), num_cl = numC
labels = {'P','PI','PID'};
figure; hold on;
for gg = 1:3
    g = gainList(gg,:);
    numC = [g(3) g(1) g(2)];      % Kd s^2 + Kp s + Ki
    % denC = s
    % open loop: numC / (s*(s^2+s+1))
    denOL = [1 1 1 0];
    numCL = numC;
    denCL = denOL + [zeros(1, numel(denOL)-numel(numC)) numC];
    sys = tf(numCL, denCL);
    [yy, tt] = step(sys);
    plot(tt, yy);
end
hold off; title('PID step response');
legend('P','PI','PID'); grid on;

% --- 61. Root locus --------------------------------------------------------
% plant: 1/(s^2+2s+2). Closed-loop den = den + K, num = K.
figure; hold on;
for K = [0.2 0.5 1 2 4]
    denCL = [1 2 2 + K];
    pr = roots(denCL);
    plot(real(pr), imag(pr), 'x');
end
hold off; title('Root locus: closed-loop poles vs K');
xlabel('Re'); ylabel('Im'); grid on;

% --- 62. Nyquist criterion --------------------------------------------------
wNy = logspace(-2, 2, 400);
[ magNy, phNy ] = bode(tf(1, [1 1 1]), wNy);
zNy = magNy(:) .* exp(1i * phNy(:) * pi/180);
figure; plot(real(zNy), imag(zNy)); hold on;
plot(-1, 0, 'rx', 'MarkerSize', 12, 'LineWidth', 2);
hold off; title('Nyquist plot vs critical point (-1, j0)');
xlabel('Re'); ylabel('Im'); grid on;

% --- 63. Planar 2-link forward kinematics ------------------------------------
L1 = 1; L2 = 0.7;
th1 = linspace(0, pi/2, 30);
th2 = linspace(0, pi, 30);
figure; hold on;
for k = 1:numel(th1)
    xj = L1*cos(th1(k)); yj = L1*sin(th1(k));
    xe = xj + L2*cos(th1(k) + th2(k));
    ye = yj + L2*sin(th1(k) + th2(k));
    plot([0 xj xe], [0 yj ye], 'b-');
    plot(xe, ye, 'r.');
end
plot(0, 0, 'ko', 'MarkerFaceColor', 'k');
title('2-link manipulator: workspace sweep'); axis equal; grid on;
