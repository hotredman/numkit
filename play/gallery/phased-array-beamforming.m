%{
@play
{
  "title": "Фазированная антенная решётка (Beamforming)",
  "description": "Интерактивный расчёт диаграммы направленности и подавления боковых лепестков",
  "groups": [
    {
      "name": "Геометрия антенны",
      "controls": [
        { "var": "N", "label": "Число элементов", "min": 8, "max": 64, "step": 2, "default": 16 },
        { "var": "d", "label": "Шаг решётки (d/λ)", "min": 0.1, "max": 1.5, "step": 0.05, "default": 0.5 }
      ]
    },
    {
      "name": "Управление лучом",
      "controls": [
        { "var": "theta0", "label": "Угол поворота", "min": -60, "max": 60, "step": 1, "unit": "°", "default": 25 },
        { "var": "win", "label": "Оконная функция", "type": "select", "options": ["rect", "hamming", "chebwin"], "default": "hamming" }
      ]
    }
  ]
}
%}
% =========================================================================
% Фазированная решётка: array factor в polar-координатах
% =========================================================================
N = 16;
theta0 = 25;
d = 0.5;
win = 'hamming';
% Весовые коэффициенты
if strcmp(win, 'chebwin')
    w = chebwin(N, 30);
elseif strcmp(win, 'hamming')
    w = hamming(N);
else
    w = ones(N, 1);
end
% Угловая развёртка и множитель решётки
theta = -90:0.5:90;
ph = 2 * pi * d * (sin(theta * pi / 180) - sin(theta0 * pi / 180));
AF = zeros(1, numel(theta));
for k = 1:N
    AF = AF + w(k) * exp(1i * (k - 1) * ph);
end
mag = abs(AF) / sum(w);
polarplot(theta * pi / 180, mag);
