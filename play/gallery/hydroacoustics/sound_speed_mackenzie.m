% Маккензи 1981
% 9-членный полином T/S/D

Tmax = 30; S = 35; D = 10;
T = 2:1:Tmax;
c = 1448.96 + 4.591*T - 5.304e-2*T.^2 + 2.374e-4*T.^3 + 1.340*(S-35) + 1.630e-2*D + 1.675e-7*D^2 - 1.025e-2*T*(S-35) - 7.139e-13*T*D^3;
figure;
plot(T, c);
title('Mackenzie 1981'); xlabel('T °C'); ylabel('c m/s'); grid on;

%{
@play
{
 "title": "Mackenzie 1981",
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
