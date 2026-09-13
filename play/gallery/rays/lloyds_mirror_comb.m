% Гребёнка Ллойда
% Частотная фильтрация у поверхности

z = 20; r = 500; c = 1500;
fz = linspace(10, 2000, 400);
dp = 2*z*r/sqrt(r^2+z^2);
figure;
plot(fz, abs(cos(pi*fz*dp/c)));
title('Frequency comb at z=20м'); xlabel('f Гц'); grid on;

%{
@play
{
 "title": "Lloyd's mirror comb",
 "groups": [
  {
   "name": "Geometry",
   "controls": [
    {
     "var": "z",
     "label": "Depth m",
     "min": 5,
     "max": 100,
     "step": 5,
     "default": 20
    },
    {
     "var": "r",
     "label": "Range m",
     "min": 100,
     "max": 2000,
     "step": 50,
     "default": 500
    }
   ]
  }
 ]
}
%}
