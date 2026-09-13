% КИХ + Кайзер
% β: переходная полоса vs подавление

beta = 5; fc = 0.25;
Nt = 64;
bK = fir1(Nt, fc, kaiser(Nt+1, beta));
[HK, wfK] = freqz(bK, 1, 512);
figure;
plot(wfK/(2*pi), 20*log10(abs(HK)));
title('FIR Kaiser'); xlabel('norm f'); grid on;

%{
@play
{
 "title": "КИХ + Кайзер",
 "groups": [
  {
   "name": "Синтез",
   "controls": [
    {
     "var": "beta",
     "label": "β",
     "min": 1,
     "max": 10,
     "step": 1,
     "default": 5
    },
    {
     "var": "fc",
     "label": "Срез ×fs",
     "min": 0.1,
     "max": 0.45,
     "step": 0.05,
     "default": 0.25
    }
   ]
  }
 ]
}
%}
