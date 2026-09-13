% Half-wave dipole
% F(theta) = cos(pi/2*cos(theta)) / sin(theta)

th = linspace(0, 2*pi, 361);
pat = abs(cos(pi/2*cos(th)) ./ max(1e-9, sin(th)));  % normalized (peak 1)
figure;
polarplot(th, pat);
title('Half-wave dipole');

%{
@play
{
 "title": "Half-wave dipole",
 "groups": []
}
%}
