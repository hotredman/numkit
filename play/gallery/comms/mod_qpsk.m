% QPSK
% 4 точки, I/Q-квадратуры

noise = 0.2; Ns = 200;
ph = pi/4 + (0:3)*pi/2;
sym = exp(1i*ph(randi(4, 1, Ns)));
rx = sym + (randn(1,Ns) + 1i*randn(1,Ns))*noise;
figure;
plot(real(rx), imag(rx), '.');
title('QPSK with noise'); grid on;

%{
@play
{
 "title": "QPSK",
 "groups": [
  {
   "name": "Channel",
   "controls": [
    {
     "var": "noise",
     "label": "Noise σ",
     "min": 0.05,
     "max": 0.6,
     "step": 0.05,
     "default": 0.2
    }
   ]
  }
 ]
}
%}
