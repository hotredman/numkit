% Резонанс осциллятора
% АЧХ, фаза, добротность

zeta = 0.1;
wn = 2*pi*2;
wr = linspace(0.1, 2.3*wn, 400);
Hr = 1./sqrt((1-(wr./wn).^2).^2 + (2*zeta*wr/wn).^2);
figure;
plot(wr/wn, Hr);
title('Resonance, \zeta'); xlabel('\omega/\omega_n'); grid on;

%{
@play
{
 "title": "Резонанс осциллятора",
 "groups": [
  {
   "name": "Демпфирование",
   "controls": [
    {
     "var": "zeta",
     "label": "ζ",
     "min": 0.02,
     "max": 0.8,
     "step": 0.02,
     "default": 0.1
    }
   ]
  }
 ]
}
%}
