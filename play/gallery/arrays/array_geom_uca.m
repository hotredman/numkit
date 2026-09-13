% Кольцевая решётка (UCA)
% Всенаправленное сканирование без деформации луча

Ne = 24;
R10 = 10;
R = R10 / 10;
th = linspace(0, 2*pi, 361);
AF = zeros(1, numel(th));
for n = 1:Ne
    an = 2*pi*(n-1)/Ne;
    AF = AF + exp(1i*2*pi*R*(cos(an).*cos(th) + sin(an).*sin(th)));
end
figure;
polarplot(th, abs(AF)/Ne);
title('UCA ring pattern');

%{
@play
{
 "title": "Кольцевая решётка (UCA)",
 "groups": [
  {
   "name": "Геометрия",
   "controls": [
    {
     "var": "Ne",
     "label": "Элементов",
     "min": 8,
     "max": 48,
     "step": 4,
     "default": 24
    },
    {
     "var": "R10",
     "label": "R/λ ×10",
     "min": 2,
     "max": 20,
     "step": 1,
     "default": 10
    }
   ]
  }
 ]
}
%}
