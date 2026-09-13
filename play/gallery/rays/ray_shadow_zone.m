% Зона тени
% Отрицательный градиент клонит лучи ко дну

g10 = -2;
g = g10/10; c0 = 1500;
p = cosd(5)/c0;
zr2 = linspace(0, 400, 500);
rr2 = zeros(size(zr2));
for k = 1:numel(zr2)
    cz = c0 + g*zr2(k);
    s2 = 1 - (p*cz)^2;
    if s2 > 0
        rr2(k) = sqrt(s2)/(-p*g);
    else
        rr2(k) = NaN;
    end
end
figure;
plot(rr2, zr2);
title('Shadow zone: rays bending down');
xlabel('range м'); ylabel('z м'); grid on;

%{
@play
{
 "title": "Shadow zone",
 "groups": [
  {
   "name": "Gradient",
   "controls": [
    {
     "var": "g10",
     "label": "dc/dz ×10⁻¹",
     "min": -5,
     "max": -1,
     "step": 1,
     "default": -2
    }
   ]
  }
 ]
}
%}
