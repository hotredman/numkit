% Полуволновой диполь
% F(θ) = cos(π/2·cosθ)/sinθ

th = linspace(0, 2*pi, 361);
figure;
polarplot(th, abs(cos(pi/2*cos(th)) ./ max(1e-9, sin(th))));
title('Half-wave dipole');

%{
@play
{
 "title": "Полуволновой диполь",
 "groups": []
}
%}
