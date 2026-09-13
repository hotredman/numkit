% Диск Эйри
% Критерий Рэлея

u = linspace(1e-6, 15, 400);
airy = (2*besselj(1, u)./u).^2;
figure;
plot(u, airy);
title('Airy pattern [2J_1(u)/u]^2');
xlabel('u'); ylabel('intensity'); grid on;

%{
@play
{
 "title": "Airy disk",
 "groups": []
}
%}
