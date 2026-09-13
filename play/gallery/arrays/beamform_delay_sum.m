% Delay-and-Sum beamformer
% The classic phase-shifter sum

N = 16; theta0 = 0;
theta = -90:0.2:90;
AF = zeros(1, numel(theta));
for k = 1:N
    AF = AF + exp(1i*pi*(k-1)*(sind(theta) - sind(theta0)));
end
pat = abs(AF) / N;                 % normalized beam pattern (peak 1)
figure;
polarplot(theta*pi/180, pat);
title('Delay-and-Sum');

%{
@play
{
 "title": "Delay-and-Sum",
 "groups": [
  {
   "name": "Ray",
   "controls": [
    {
     "var": "N",
     "label": "N",
     "min": 8,
     "max": 32,
     "step": 2,
     "default": 16
    },
    {
     "var": "theta0",
     "label": "θ₀",
     "min": -60,
     "max": 60,
     "step": 5,
     "default": 0
    }
   ]
  }
 ]
}
%}
