% Коппенс 1981
% Компактная модель до 4000 м

Tmax = 30; S = 35; D = 10;
T = 2:1:Tmax;
c = 1449.05 + 4.57*T - 5.21e-2*T.^2 + 2.31e-4*T.^3 + 1.33*(S-35) - 1.10e-2*T*(S-35) + 1.655e-2*D;
figure;
plot(T, c);
title('Coppens 1981'); xlabel('T °C'); ylabel('c m/s'); grid on;

%{
@play
{
 "title": "Coppens 1981",
 "groups": [
  {
   "name": "Environment",
   "controls": [
    {
     "var": "Tmax",
     "label": "T max °C",
     "min": 5,
     "max": 30,
     "step": 1,
     "default": 30
    },
    {
     "var": "S",
     "label": "Salinity",
     "min": 30,
     "max": 40,
     "step": 1,
     "default": 35
    },
    {
     "var": "D",
     "label": "Depth m",
     "min": 0,
     "max": 4000,
     "step": 100,
     "default": 10
    }
   ]
  }
 ]
}
%}
