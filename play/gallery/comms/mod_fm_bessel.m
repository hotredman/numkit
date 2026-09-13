% FM: спектр Бесселя
% Нуль несущей при β = 2.405

df = 240;
fs2 = 4000;
ttF = 0:1/fs2:0.1;
msgF = sin(2*pi*100*ttF);
instF = 2000 + df*msgF;
phF = 2*pi*cumsum(instF)/fs2;
sigF2 = sin(phF);
[SpF, fF] = pwelch(sigF2, hann(512), 256, 1024, fs2);
figure;
plot(fF, 20*log10(SpF));
title('FM spectrum'); xlabel('f Гц'); grid on;

%{
@play
{
 "title": "FM: спектр Бесселя",
 "groups": [
  {
   "name": "Модуляция",
   "controls": [
    {
     "var": "df",
     "label": "Девиация Гц",
     "min": 50,
     "max": 500,
     "step": 10,
     "default": 240
    }
   ]
  }
 ]
}
%}
