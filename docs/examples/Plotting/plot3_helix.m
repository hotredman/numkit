% plot3 Helix — 3-D parametric curve
% Draw a 3-D helix by sweeping (cos t, sin t, t). Try the orbit
% camera: drag with the mouse to rotate, wheel to dolly in/out.
import compat.*;

clear
close all

t = linspace(0, 6*pi, 200);
x = cos(t);
y = sin(t);
z = t / (2*pi);

figure
plot3(x, y, z)
title('3-D Helix')
xlabel('cos(t)')
ylabel('sin(t)')
zlabel('t / 2\pi')
grid on
view(-37.5, 30)
