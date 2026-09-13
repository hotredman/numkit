% Spatial null steering
% Deep null toward the jammer by pattern subtraction:
%   AFnull = AF(theta0-steered) - alpha * AF(thetaJ-steered),
%   alpha = AF0(thetaJ)/AFJ(thetaJ)  ->  exact zero at thetaJ.
thetaJ = -40;
N = 16;
theta0 = 20;
theta = -90:0.2:90;
idxJ = round((thetaJ + 90) / 0.2) + 1;   % grid index of the jammer angle
% Beam steered to the target angle theta0
AF0 = zeros(1, numel(theta));
% Reference beam steered to the jammer angle thetaJ
AFJ = zeros(1, numel(theta));
for k = 1:N
    AF0 = AF0 + exp(1i*pi*(k-1)*(sind(theta) - sind(theta0)));
    AFJ = AFJ + exp(1i*pi*(k-1)*(sind(theta) - sind(thetaJ)));
end
alpha = AF0(idxJ) / AFJ(idxJ);           % exact-null weight
AFnull = AF0 - alpha * AFJ;
patDb = 20*log10(abs(AFnull)/N + 1e-12); % dB, 0 dB at the main beam
figure;
plot(theta, patDb);
title('Null steering at \theta_J'); xlabel('\theta deg'); ylabel('dB'); grid on;

%{
@play
{
 "title": "Spatial nulls",
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
    }
   ]
  }
 ]
}
%}
