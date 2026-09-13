% Моноимпульсный пеленгатор
% Σ/Δ диаграммы и пеленгационная характеристика

thetaT = 3;
theta = -10:0.1:10;
sig = cos(pi*2*sind(theta));
dif = sin(pi*2*sind(theta));
mono = dif ./ max(1e-9, sig);
figure;
plot(theta, sig, theta, dif, theta, mono);
legend('Σ (sum)', 'Δ (diff)', 'Δ/Σ');
title('Monopulse characteristics'); grid on;

%{
@play
{
 "title": "Моноимпульсный пеленгатор",
 "groups": [
  {
   "name": "Цель",
   "controls": [
    {
     "var": "thetaT",
     "label": "Цель θ",
     "min": -10,
     "max": 10,
     "step": 1,
     "default": 3
    }
   ]
  }
 ]
}
%}
