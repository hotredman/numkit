% Критерий Найквиста
% Огибание (-1, j0)

wNy = logspace(-2, 2, 400);
[magNy, phNy] = bode(tf(1, [1 1 1]), wNy);
zNy = magNy(:).*exp(1i*phNy(:)*pi/180);
figure;
plot(real(zNy), imag(zNy));
hold on;
plot(-1, 0, 'rx', 'MarkerSize', 12, 'LineWidth', 2);
hold off;
title('Nyquist vs (-1, j0)'); xlabel('Re'); ylabel('Im'); grid on;

%{
@play
{
 "title": "Nyquist criterion",
 "groups": []
}
%}
