% Утечка спектра БПФ
% Окна при некратной частоте

fL = 50.5; fs4 = 1000; N4 = 256;
t4 = (0:N4-1)/fs4;
xL = sin(2*pi*fL*t4);
whan = hann(N4)';
wbh = blackmanharris(N4)';
YL = abs(fft(xL));
YL2 = abs(fft(xL .* whan));
YL3 = abs(fft(xL .* wbh));
figure;
plot(20*log10(YL(1:N4/2)), 20*log10(YL2(1:N4/2)), 20*log10(YL3(1:N4/2)));
legend('rect','hann','blackman-harris');
title('Spectral leakage'); grid on;

%{
@play
{
 "title": "FFT spectral leakage",
 "groups": [
  {
   "name": "Window",
   "controls": [
    {
     "var": "fL",
     "label": "signal f Hz",
     "min": 40,
     "max": 90,
     "step": 0.5,
     "default": 50.5
    }
   ]
  }
 ]
}
%}
