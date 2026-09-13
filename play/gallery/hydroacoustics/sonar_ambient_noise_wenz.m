% Шум океана (Венц)
% Судоходство / ветер / термический

fMax = 20000;
fw = logspace(1, log10(fMax), 200);
NLship = 44 - 17*log10(fw/100);
NLwind = 33 - 25*log10(fw/500);
NLth = -15 + 20*log10(fw);
figure;
loglog(fw, NLship, fw, NLwind, fw, NLth);
legend('shipping','wind','thermal');
title('Wenz ambient noise'); grid on;

%{
@play
{
 "title": "Шум океана (Венц)",
 "groups": [
  {
   "name": "Диапазон",
   "controls": [
    {
     "var": "fMax",
     "label": "f макс Гц",
     "min": 1000,
     "max": 50000,
     "step": 5000,
     "default": 20000
    }
   ]
  }
 ]
}
%}
