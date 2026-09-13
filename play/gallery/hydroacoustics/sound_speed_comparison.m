% Сравнение моделей c(z)
% Δc между четырьмя уравнениями

S = 35; D = 10;
T = 2:1:30;
cM = 1448.96 + 4.591*T - 5.304e-2*T.^2 + 2.374e-4*T.^3 + 1.34*(S-35) + 1.63e-2*D - 1.025e-2*T*(S-35);
cDG = 1402.392 + 4.63573*T - 3.5885e-2*T.^2 + 1.59964e-4*T.^3 + 1.33428*(S-35) - 1.13083e-2*T*(S-35);
cUN = 1449.2 + 4.6*T - 5.48e-2*T.^2 + 2.91e-4*T.^3 + 1.34*(S-35) - 1.02e-2*T*(S-35);
cCp = 1449.05 + 4.57*T - 5.21e-2*T.^2 + 2.31e-4*T.^3 + 1.33*(S-35) - 1.10e-2*T*(S-35);
figure;
plot(T, cM-cDG, T, cM-cUN, T, cM-cCp);
legend('M-DG','M-UN','M-Cp');
title('\Delta c between models'); grid on;

%{
@play
{
 "title": "c(z) model comparison",
 "groups": [
  {
   "name": "Environment",
   "controls": [
    {
     "var": "S",
     "label": "Salinity",
     "min": 30,
     "max": 40,
     "step": 1,
     "default": 35
    },
    {
     "var": "D",
     "label": "Depth m",
     "min": 0,
     "max": 4000,
     "step": 100,
     "default": 10
    }
   ]
  }
 ]
}
%}
