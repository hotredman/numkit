% Франсуа-Гаррисон
% Бор + MgSO₄ + вязкость воды

Tkg = 10; pH = 8.0;
f = logspace(2, 4.7, 200);
f1 = 2.8*10^((Tkg-2)/43);
aB = 0.11*f.^2*f1./(f1^2+f.^2)*1e-3;
f2 = 62*10^((Tkg+30)/100);
aMg = 0.7*f2*f.^2./(f2^2+f.^2)*1e-3;
aW = 2.9e-2*f.^2/1e3;
figure;
loglog(f, aB+aMg+aW);
title('Francois-Garrison \alpha(f)'); grid on;

%{
@play
{
 "title": "Франсуа-Гаррисон",
 "groups": [
  {
   "name": "Среда",
   "controls": [
    {
     "var": "Tkg",
     "label": "T °C",
     "min": 0,
     "max": 30,
     "step": 1,
     "default": 10
    },
    {
     "var": "pH",
     "label": "pH",
     "min": 7.5,
     "max": 8.5,
     "step": 0.1,
     "default": 8.0
    }
   ]
  }
 ]
}
%}
