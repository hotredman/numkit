% Поршень в экране
% F = 2J₁(u)/u — эталон пьезокерамики

uMax = 40;
u = linspace(1e-6, uMax, 721);
pat = 2*besselj(1, u) ./ u;
figure;
plot(u, pat);
title('Baffled piston 2J_1(u)/u'); grid on;

%{
@play
{
 "title": "Поршень в экране",
 "groups": [
  {
   "name": "Апертура",
   "controls": [
    {
     "var": "uMax",
     "label": "u макс",
     "min": 10,
     "max": 60,
     "step": 5,
     "default": 40
    }
   ]
  }
 ]
}
%}
