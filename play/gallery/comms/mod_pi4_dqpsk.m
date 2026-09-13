% π/4-DQPSK
% Без переходов через ноль

Ns = 100;
dph = pi/4*(2*randi([0 3], 1, Ns) + 1);
phPi4 = cumsum(dph);
figure;
plot(cos(phPi4), sin(phPi4), '.-');
title('\pi/4-DQPSK trajectory'); axis([-2 2 -2 2]); grid on;

%{
@play
{
 "title": "π/4-DQPSK",
 "groups": [
  {
   "name": "Символы",
   "controls": [
    {
     "var": "Ns",
     "label": "Символов",
     "min": 20,
     "max": 200,
     "step": 20,
     "default": 100
    }
   ]
  }
 ]
}
%}
