% Дель Гроссо 1974
% Высокоточный стандарт ВМФ

Tmax = 30; S = 35; D = 10;
T = 2:1:Tmax;
c = 1402.392 + 4.63573*T - 3.5885e-2*T.^2 + 1.59964e-4*T.^3 - 1.3904e-6*T.^4 + 1.33428*(S-35) + 1.60272e-2*D + 8.34076e-7*D^3 - 1.13083e-2*T*(S-35);
figure;
plot(T, c);
title('Del Grosso 1974'); xlabel('T °C'); ylabel('c m/s'); grid on;

%{
@play
{
 "title": "Del Grosso 1974",
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
