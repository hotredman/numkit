% Baffled piston
% F = 2*J1(u)/u — the piezo-ceramics reference

uMax = 40;
u = linspace(1e-6, uMax, 721);
pat = 2*besselj(1, u) ./ u;        % normalized (peak 1 at u->0)
figure;
plot(u, pat);
title('Baffled piston 2J_1(u)/u'); xlabel('u'); grid on;

%{
@play
{
 "title": "Piston in baffle",
 "groups": [
  {
   "name": "Aperture",
   "controls": [
    {
     "var": "uMax",
     "label": "u max",
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
