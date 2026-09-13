% ЮНЕСКО 1983
% Чен-Миллеро, международный стандарт

Tmax = 30; S = 35; D = 10;
T = 2:1:Tmax;
c = 1449.2 + 4.6*T - 5.48e-2*T.^2 + 2.91e-4*T.^3 + 1.34*(S-35) + 1.60e-2*D + 1.6e-7*D^2 - 1.02e-2*T*(S-35);
figure;
plot(T, c);
title('UNESCO 1983'); xlabel('T °C'); ylabel('c m/s'); grid on;

%{
@play
{
 "title": "UNESCO 1983",
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
