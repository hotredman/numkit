% Оптимальная частота
% Максимум SNR на дистанции

Rk = 5;
f = logspace(1, 4.5, 200);
a = 0.11*f.^2./(1+f.^2) + 44*f.^2./(4100+f.^2) + 2.75e-4*f.^2;
twoWay = 2*a*Rk;
figure;
semilogx(f, twoWay);
title('Two-way absorption @ 5 km'); xlabel('f Hz'); ylabel('дБ'); grid on;

%{
@play
{
 "title": "Оптимальная частота",
 "groups": [
  {
   "name": "Задача",
   "controls": [
    {
     "var": "Rk",
     "label": "Дистанция км",
     "min": 1,
     "max": 20,
     "step": 1,
     "default": 5
    }
   ]
  }
 ]
}
%}
