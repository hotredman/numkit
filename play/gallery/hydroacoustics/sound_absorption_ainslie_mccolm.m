% Эйнсли-Макколм
% Современная модель затухания

Tkg = 10;
f = logspace(2, 4.7, 200);
f1 = 2.8*10^((Tkg-2)/43);
f2 = 62*10^((Tkg+30)/100);
a = 0.106*f.^2*f1./(f1^2+f.^2)*1e-3 + 0.6*f2*f.^2./(f2^2+f.^2)*1e-3 + 2.75e-4*f.^2;
figure;
loglog(f, a);
title('Ainslie-McColm \alpha(f)'); grid on;

%{
@play
{
 "title": "Ainslie-McColm",
 "groups": [
  {
   "name": "Environment",
   "controls": [
    {
     "var": "Tkg",
     "label": "T °C",
     "min": 0,
     "max": 30,
     "step": 1,
     "default": 10
    }
   ]
  }
 ]
}
%}
