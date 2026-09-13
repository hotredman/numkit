%{
@play
{
  "title": "2D-гауссиана (imagesc)",
  "description": "Тепловая карта двумерного гауссова распределения",
  "groups": [
    {
      "name": "Параметры распределения",
      "controls": [
        { "var": "sigma", "label": "σ", "min": 0.3, "max": 2.0, "step": 0.1, "default": 0.8 },
        { "var": "n", "label": "Сетка N×N", "min": 16, "max": 96, "step": 16, "default": 48 },
        { "var": "cm", "label": "Палитра", "type": "select", "options": ["hot", "parula", "jet", "gray"], "default": "parula" }
      ]
    }
  ]
}
%}
% 2D-гауссиана на сетке N×N
sigma = 0.8;
n = 48;
xs = linspace(-3, 3, n);
Z = zeros(n, n);
for i = 1:n
    for j = 1:n
        Z(i, j) = exp(-(xs(i)^2 + xs(j)^2) / (2 * sigma^2));
    end
end
imagesc(xs, xs, Z);
colorbar;
title('2D Gaussian');
xlabel('x');
ylabel('y');
