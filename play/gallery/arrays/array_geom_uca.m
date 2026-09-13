% Circular array (UCA)
% Omnidirectional scanning without beam distortion

Ne = 24;
R10 = 10;
R = R10 / 10;
th = linspace(0, 2*pi, 361);
AF = zeros(1, numel(th));
for n = 1:Ne
    an = 2*pi*(n-1)/Ne;
    AF = AF + exp(1i*2*pi*R*(cos(an).*cos(th) + sin(an).*sin(th)));
end
pat = abs(AF) / Ne;                 % normalized beam pattern (peak 1)
figure;
polarplot(th, pat);
title('UCA ring pattern');

%{
@play
{
 "title": "Circular array (UCA)",
 "groups": [
  {
   "name": "Geometry",
   "controls": [
    {
     "var": "Ne",
     "label": "Elements",
     "min": 8,
     "max": 48,
     "step": 4,
     "default": 24
    },
    {
     "var": "R10",
     "label": "R/λ ×10",
     "min": 2,
     "max": 20,
     "step": 1,
     "default": 10
    }
   ]
  }
 ]
}
%}
