% BFSK
% Ортогональности Δf=1/2T и Δf=1/T

f2 = 1500;
tb = 1e-3; f1 = 2000;
tg = 0:tb/100:tb;
s1 = sin(2*pi*f1*tg);
s2c = sin(2*pi*f2*tg);
s2o = sin(2*pi*(f1-500)*tg);
figure;
plot(tg, s1, tg, s2c, tg, s2o);
legend('f1','\Deltaf=1/2T','\Deltaf=1/T');
title('BFSK orthogonality'); grid on;

%{
@play
{
 "title": "BFSK",
 "groups": [
  {
   "name": "Frequencies",
   "controls": [
    {
     "var": "f2",
     "label": "f₂ Hz",
     "min": 1200,
     "max": 1800,
     "step": 50,
     "default": 1500
    }
   ]
  }
 ]
}
%}
