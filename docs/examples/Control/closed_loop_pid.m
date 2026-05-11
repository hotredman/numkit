% closed_loop_pid.m
% Build a PID controller and close the loop around a 2nd-order plant
% G(s) = 1 / (s^2 + s + 1). Compare open-loop step vs closed-loop step
% with a hand-tuned P / PI / PID. Open-loop product C*G is built by
% polynomial multiplication (conv); closed loop = CG / (1 + CG).

import compat.*;

clear
close all

% Plant.
numG = 1;
denG = [1, 1, 1];
G = tf(numG, denG);

% Controller numerators / denominators.
%   P:   C = Kp                            → num = [Kp],         den = 1
%   PI:  C = (Kp s + Ki) / s               → num = [Kp Ki],     den = [1 0]
%   PID: C = (Kd s^2 + Kp s + Ki) / s      → num = [Kd Kp Ki],  den = [1 0]
ctls = struct();
ctls.P   = struct('num', 4,            'den', 1);
ctls.PI  = struct('num', [4, 1],       'den', [1, 0]);
ctls.PID = struct('num', [0.6, 4, 1],  'den', [1, 0]);

t = linspace(0, 12, 400);
y_open = step(G, t);
names  = {'P', 'PI', 'PID'};
y      = cell(1, 3);

for k = 1:3
    c = ctls.(names{k});
    % CG = (numC * numG) / (denC * denG)
    numCG = conv(c.num, numG);
    denCG = conv(c.den, denG);
    % T = CG / (1 + CG) = numCG / (denCG + numCG); pad numerator to align.
    n_pad = [zeros(1, numel(denCG) - numel(numCG)), numCG];
    T = tf(numCG, denCG + n_pad);
    y{k} = step(T, t);
end

figure;
plot(t, y_open, 'k--', t, y{1}, 'b', t, y{2}, 'g', t, y{3}, 'r', 'LineWidth', 1.5);
hold on;
yline(1, 'k:'); hold off;
grid on;
xlabel('time (s)'); ylabel('output');
legend('open loop', 'P only', 'PI', 'PID', 'setpoint = 1', 'location', 'southeast');
title('Closed-loop step response — P / PI / PID');
