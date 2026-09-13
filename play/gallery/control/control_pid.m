% ПИД-регулятор
% Kp/Ki/Kd и переходная характеристика

Kp = 2; Ki = 1; Kd = 0.1;
numC = [Kd Kp Ki];
denOL = [1 1 1 0];
denCL = denOL + [zeros(1, numel(denOL)-numel(numC)) numC];
sys = tf(numC, denCL);
[yy, tt] = step(sys);
figure;
plot(tt, yy);
title('PID step response'); grid on;

%{
@play
{
 "title": "ПИД-регулятор",
 "groups": [
  {
   "name": "Регулятор",
   "controls": [
    {
     "var": "Kp",
     "label": "Kp",
     "min": 0.5,
     "max": 8,
     "step": 0.5,
     "default": 2
    },
    {
     "var": "Ki",
     "label": "Ki",
     "min": 0,
     "max": 4,
     "step": 0.5,
     "default": 1
    },
    {
     "var": "Kd",
     "label": "Kd",
     "min": 0,
     "max": 1,
     "step": 0.05,
     "default": 0.1
    }
   ]
  }
 ]
}
%}
