% ASK / OOK
% Амплитудная манипуляция, порог компаратора

mDepth = 0.8; fC = 2000; fs = 10e3; symR = 100;
bits = [1 0 1 1 0 1 0 0];
seq = repelem(bits, fs/symR);
t = (0:numel(seq)-1)/fs;
modSig = (1 + mDepth*(seq - 0.5)) .* sin(2*pi*fC*t);
thr = 0.5*max(abs(modSig))*ones(1, numel(modSig));
figure;
plot(t, modSig, t, thr, 'r--');
title('ASK/OOK waveform'); grid on;

%{
@play
{
 "title": "ASK / OOK",
 "groups": [
  {
   "name": "Сигнал",
   "controls": [
    {
     "var": "mDepth",
     "label": "Индекс m",
     "min": 0.2,
     "max": 1.0,
     "step": 0.1,
     "default": 0.8
    },
    {
     "var": "fC",
     "label": "Несущая Гц",
     "min": 500,
     "max": 4000,
     "step": 500,
     "default": 2000
    }
   ]
  }
 ]
}
%}
