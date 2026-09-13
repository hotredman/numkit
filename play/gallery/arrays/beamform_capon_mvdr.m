% Capon / MVDR beamformer
% Adaptive interference suppression

thetaJ = -40; Jpow = 50;
theta = -90:1:90;
N = 16;
% Simplified model: response = 1 / (1 + Jpow * exp(-(sep/0.3)^2)),
% a Gaussian-notch stand-in for the MVDR quiescent pattern.
sep = abs(sind(theta) - sind(thetaJ));
resp = 1 ./ (1 + Jpow * exp(-((sep/0.3).^2)));   % normalized (0 dB off-notch)
patDb = 20*log10(resp);
figure;
plot(theta, patDb);
title('MVDR notch at jammer'); xlabel('\theta deg'); ylabel('dB'); grid on;

%{
@play
{
 "title": "Capon / MVDR",
 "groups": [
  {
   "name": "Interference",
   "controls": [
    {
     "var": "thetaJ",
     "label": "Interference angle",
     "min": -80,
     "max": 80,
     "step": 5,
     "default": -40
    },
    {
     "var": "Jpow",
     "label": "Interference power",
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
