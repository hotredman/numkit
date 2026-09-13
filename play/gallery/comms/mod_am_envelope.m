% AM: огибающая
% m ≤ 1 норма; m > 1 — перемодуляция

m = 0.8;
fs2 = 4000;
ta = 0:1/fs2:0.0625;
msg = cos(2*pi*300*ta);
am = (1 + m*msg).*sin(2*pi*2000*ta);
np2 = 2^nextpow2(numel(am));
env = abs(hilbert([am(:); zeros(np2 - numel(am), 1)]));
env = env(1:numel(am));
figure;
plot(ta, am, ta, env, 'r');
title('AM envelope'); grid on;

%{
@play
{
 "title": "AM: огибающая",
 "groups": [
  {
   "name": "Модуляция",
   "controls": [
    {
     "var": "m",
     "label": "Индекс m",
     "min": 0.1,
     "max": 1.5,
     "step": 0.1,
     "default": 0.8
    }
   ]
  }
 ]
}
%}
