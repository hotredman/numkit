% Капон / MVDR
% Адаптивное подавление помехи

thetaJ = -40; Jpow = 50;
theta = -90:1:90;
N = 16;
% Simplified: spectral weight = 1 / (1 + Jpow*sinc^2 separation)
sep = abs(sind(theta) - sind(thetaJ));
resp = 1 ./ (1 + Jpow * exp(-((sep/0.3).^2)));
figure;
plot(theta, 20*log10(resp));
title('MVDR notch at jammer'); ylabel('dB'); grid on;

%{
@play
{
 "title": "Капон / MVDR",
 "groups": [
  {
   "name": "Помеха",
   "controls": [
    {
     "var": "thetaJ",
     "label": "Угол помехи",
     "min": -80,
     "max": 80,
     "step": 5,
     "default": -40
    },
    {
     "var": "Jpow",
     "label": "Мощность помехи",
     "min": 1,
     "max": 100,
     "step": 5,
     "default": 50
    }
   ]
  }
 ]
}
%}
