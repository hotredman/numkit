% Transmission Loss
% Сферическое/цилиндрическое расхождение

Rmax = 10000; fK = 5;
r = linspace(10, Rmax, 200);
ka = 0.07*fK;
figure;
semilogx(r, 20*log10(r)+ka*r/1000, r, 10*log10(r)+ka*r/1000);
legend('spherical','cylindrical');
title('Transmission loss'); xlabel('r m'); ylabel('dB'); grid on;

%{
@play
{
 "title": "Transmission Loss",
 "groups": [
  {
   "name": "Параметры",
   "controls": [
    {
     "var": "Rmax",
     "label": "R макс м",
     "min": 1000,
     "max": 20000,
     "step": 1000,
     "default": 10000
    },
    {
     "var": "fK",
     "label": "f кГц",
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
