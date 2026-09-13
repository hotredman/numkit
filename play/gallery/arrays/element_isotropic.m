% Isotropic element
% Point-source monopole F(theta) = 1

th = linspace(0, 2*pi, 361);
pat = ones(1, numel(th));          % unity (already normalized)
figure;
polarplot(th, pat);
title('Isotropic F(\theta)=1');

%{
@play
{
 "title": "Isotropic element",
 "groups": []
}
%}
