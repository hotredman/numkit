% Формула Торпа
% Классическое затухание 0.1–10 кГц

fMax = 50;
f = logspace(1, log10(fMax*1000), 200);
a = 0.11*f.^2./(1+f.^2) + 44*f.^2./(4100+f.^2) + 2.75e-4*f.^2 + 0.003;
figure;
loglog(f, a);
title('Thorp \alpha(f) dB/km'); xlabel('f Hz'); grid on;

%{
@play
{
 "title": "Thorp formula",
 "groups": [
  {
   "name": "Range",
   "controls": [
    {
     "var": "fMax",
     "label": "f max kHz",
     "min": 10,
     "max": 100,
     "step": 10,
     "default": 50
    }
   ]
  }
 ]
}
%}
