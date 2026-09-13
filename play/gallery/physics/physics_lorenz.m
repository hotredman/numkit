% Аттрактор Лоренца
% Хаос при росте ρ (ode45)

rho = 28; sigma = 10; beta = 8/3;
fL = @(t, s) [sigma*(s(2)-s(1)); s(1)*(rho-s(3))-s(2); s(1)*s(2)-beta*s(3)];
[tL, yL] = ode45(fL, [0 20], [1 1 1]);
figure;
plot3(yL(:,1), yL(:,2), yL(:,3));
title('Lorenz attractor'); xlabel('x'); ylabel('y'); zlabel('z'); grid on;

%{
@play
{
 "title": "Аттрактор Лоренца",
 "groups": [
  {
   "name": "Система",
   "controls": [
    {
     "var": "rho",
     "label": "ρ",
     "min": 5,
     "max": 45,
     "step": 1,
     "default": 28
    }
   ]
  }
 ]
}
%}
