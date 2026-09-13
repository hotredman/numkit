% Рефракция Снеллиуса
% Кривизна луча в градиенте c

g10 = 1; th0 = 5;
g = g10/10; c0 = 1500;
p = cosd(th0)/c0;
zr2 = linspace(0, 400, 500);
rr = zeros(size(zr2));
for k = 1:numel(zr2)
    cz = c0 + g*zr2(k);
    s2 = 1 - (p*cz)^2;
    if s2 > 0
        rr(k) = sqrt(s2)/(p*g);
    else
        rr(k) = NaN;
    end
end
figure;
plot(rr, zr2);
title('Ray in +gradient'); xlabel('range м'); ylabel('z м'); grid on;

%{
@play
{
 "title": "Рефракция Снеллиуса",
 "groups": [
  {
   "name": "Градиент",
   "controls": [
    {
     "var": "g10",
     "label": "dc/dz ×10⁻¹",
     "min": -5,
     "max": 5,
     "step": 1,
     "default": 1
    },
    {
     "var": "th0",
     "label": "Угол выхода",
     "min": -10,
     "max": 10,
     "step": 1,
     "default": 5
    }
   ]
  }
 ]
}
%}
