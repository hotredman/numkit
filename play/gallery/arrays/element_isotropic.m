% Изотропный элемент
% Точечный монополь F(θ)=1

th = linspace(0, 2*pi, 361);
figure;
polarplot(th, ones(1, numel(th)));
title('Isotropic F(\theta)=1');

%{
@play
{
 "title": "Изотропный элемент",
 "groups": []
}
%}
