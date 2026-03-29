clear

% Beam Steering — Phased array antenna
% Demonstrates steering a linear array beam to 60 degrees
% by applying a progressive phase shift.

N = 8;
d = 0.5;  % element spacing in wavelengths
theta = linspace(0, 2*pi, 720);

% Unsteered array factor
AF = zeros(1, length(theta));
for n = 0:N-1
    AF = AF + exp(1i * 2 * pi * d * n * cos(theta));
end
AF = abs(AF) / max(abs(AF));

figure(1)
polarplot(theta, AF)
title('Broadside pattern (no steering)')
grid on

% Steer to 60 degrees
theta0 = pi / 3;
beta = -2 * pi * d * cos(theta0);

AF2 = zeros(1, length(theta));
for n = 0:N-1
    AF2 = AF2 + exp(1i * n * (2 * pi * d * cos(theta) + beta));
end
AF2 = abs(AF2) / max(abs(AF2));

figure(2)
polarplot(theta, AF2)
title('Beam steered to 60 deg')
grid on
