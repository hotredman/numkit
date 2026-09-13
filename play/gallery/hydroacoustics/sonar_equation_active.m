% Активный сонар
% SL − 2TL + TS − (NL − DI) ≥ DT

SL = 220; TS = 10; NL = 60; DI = 20;
DT = 5;
TLok = (SL + TS - NL - DI + DT)/2;
r = logspace(1, 5, 200);
TLs = 20*log10(r) + 0.35*r/1000;
figure;
semilogx(r, TLs);
hold on;
yline(TLok, 'r--');
hold off;
title('Active sonar budget'); xlabel('r м'); ylabel('TL дБ'); grid on;

%{
@play
{
 "title": "Active sonar",
 "groups": [
  {
   "name": "Budget",
   "controls": [
    {
     "var": "SL",
     "label": "SL dB",
     "min": 180,
     "max": 240,
     "step": 5,
     "default": 220
    },
    {
     "var": "TS",
     "label": "TS dB",
     "min": -10,
     "max": 30,
     "step": 5,
     "default": 10
    },
    {
     "var": "NL",
     "label": "NL dB",
     "min": 40,
     "max": 80,
     "step": 5,
     "default": 60
    }
   ]
  }
 ]
}
%}
