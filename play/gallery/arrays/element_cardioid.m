% Кардиоидный элемент
% F = |1 + β·cos θ| — подавление заднего лепестка

beta = 10;
th = linspace(0, 2*pi, 361);
figure;
polarplot(th, abs(1 + beta/10*cos(th)));
title('Cardioid element');

%{
@play
{
 "title": "Кардиоидный элемент",
 "groups": [
  {
   "name": "Форма",
   "controls": [
    {
     "var": "beta",
     "label": "β ×10",
     "min": 0,
     "max": 10,
     "step": 1,
     "default": 10
    }
   ]
  }
 ]
}
%}
