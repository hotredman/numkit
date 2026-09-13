% Кинематика 2-звенника
% Рабочая зона схвата

L1 = 1.0; L2 = 0.7;
th1 = linspace(0, pi/2, 30);
th2 = linspace(0, pi, 30);
figure;
hold on;
for k = 1:numel(th1)
    xj = L1*cos(th1(k)); yj = L1*sin(th1(k));
    xe = xj + L2*cos(th1(k) + th2(k));
    ye = yj + L2*sin(th1(k) + th2(k));
    plot([0 xj xe], [0 yj ye], 'b-');
    plot(xe, ye, 'r.');
end
plot(0, 0, 'ko', 'MarkerFaceColor', 'k');
title('2-link FK workspace'); axis equal; grid on;

%{
@play
{
 "title": "Кинематика 2-звенника",
 "groups": [
  {
   "name": "Манипулятор",
   "controls": [
    {
     "var": "L1",
     "label": "L₁ м",
     "min": 0.5,
     "max": 1.5,
     "step": 0.1,
     "default": 1.0
    },
    {
     "var": "L2",
     "label": "L₂ м",
     "min": 0.3,
     "max": 1.2,
     "step": 0.1,
     "default": 0.7
    }
   ]
  }
 ]
}
%}
