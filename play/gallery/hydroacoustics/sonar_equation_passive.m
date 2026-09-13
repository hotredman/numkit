% Пассивный сонар
% SL − TL − (NL − DI) ≥ DT

SLp = 180; DI = 20;
DT = 5;
TLmax = SLp - (60 - DI + DT);
r = logspace(1, 5, 200);
TLc = 10*log10(r) + 0.07*r/1000;
figure;
semilogx(r, TLc);
hold on;
yline(TLmax, 'r--');
hold off;
title('Passive sonar budget'); xlabel('r м'); ylabel('TL дБ'); grid on;

%{
@play
{
 "title": "Passive sonar",
 "groups": [
  {
   "name": "Budget",
   "controls": [
    {
     "var": "SLp",
     "label": "Target SL dB",
     "min": 150,
     "max": 200,
     "step": 5,
     "default": 180
    },
    {
     "var": "DI",
     "label": "DI dB",
     "min": 5,
     "max": 30,
     "step": 5,
     "default": 20
    }
   ]
  }
 ]
}
%}
