% Polar Config — Theta direction, zero location, rlim
% Customize polar plot orientation and radial limits.
%
%   thetazero('top')           — 0 degrees at top (compass style)
%   thetadir('clockwise')      — angles increase clockwise
%   rlim([rmin rmax])          — set radial axis limits
clear
close all

theta = linspace(0, 2*pi, 360);
rho = abs(cos(2 * theta));  % four-leaf clover

% Default orientation (0 = right, counterclockwise)
figure(1)
polarplot(theta, rho)
title('Default Polar (0 = right, CCW)')
grid on

% Compass style (0 = top, clockwise)
figure(2)
polarplot(theta, rho)
thetazero('top')
thetadir('clockwise')
title('Compass Style (0 = top, CW)')
grid on

% With rlim
figure(3)
polarplot(theta, rho)
rlim([0 0.8])
title('Four-leaf clover, rlim [0, 0.8]')
grid on
