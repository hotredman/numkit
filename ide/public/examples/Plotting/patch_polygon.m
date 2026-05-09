% patch — Filled polygons
% patch(X, Y) shades a single polygon; matrix inputs draw one
% polygon per column. Color accepts a single-char ('r', 'g', 'b'),
% a "#rrggbb" string, or a 3-element [r g b] triplet in [0, 1].
import compat.*;

clear
close all

% Triangle.
T_x = [0 1 0.5];
T_y = [0 0 1];

% Square offset to the right.
S_x = [1.5 2.5 2.5 1.5];
S_y = [0   0   1   1];

% Hexagon offset upward, defined by parameter sweep.
theta = linspace(0, 2*pi, 7);
theta = theta(1:6);
H_x = 0.5 * cos(theta) + 1;
H_y = 0.5 * sin(theta) + 2;

figure
patch(T_x, T_y, 'r')
hold('on')
patch(S_x, S_y, [0.2 0.6 0.9])
patch(H_x, H_y, '#ffd166')
title('patch — Triangle, Square, Hexagon')
xlabel('X')
ylabel('Y')
axis('equal')
grid on
