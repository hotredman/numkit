% Short dipole
% F(theta) = |cos(theta)|

th = linspace(0, 2*pi, 361);
pat = abs(cos(th));                % normalized (peak 1)
figure;
polarplot(th, pat);
title('Short dipole F=cos\theta');

%{
@play
{
 "title": "Short dipole",
 "groups": []
}
%}
