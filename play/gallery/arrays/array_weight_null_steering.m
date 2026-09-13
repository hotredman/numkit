% Пространственные нули
% Глубокий провал в сторону помехи

thetaJ = -40;
N = 16;
theta0 = 20;
theta = -90:0.2:90;
AF = zeros(1, numel(theta));
for k = 1:N
    AF = AF + exp(1i*pi*(k-1)*(sind(theta) - sind(theta0))) ...
            - exp(1i*pi*(k-1)*(sind(theta) - sind(thetaJ)));
end
figure;
plot(theta, 20*log10(abs(AF)/N + 1e-12));
title('Null steering at \theta_J'); ylabel('dB'); grid on;

%{
@play
{
 "title": "Пространственные нули",
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
    }
   ]
  }
 ]
}
%}
