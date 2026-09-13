% Короткий диполь
% F(θ) = cos θ

th = linspace(0, 2*pi, 361);
figure;
polarplot(th, abs(cos(th)));
title('Short dipole F=cos\theta');

%{
@play
{
 "title": "Short dipole",
 "groups": []
}
%}
