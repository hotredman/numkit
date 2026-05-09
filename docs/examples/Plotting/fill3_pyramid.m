% fill3 — 3-D filled polygons
% Build a square-base pyramid by emitting each triangular face as a
% column of (X, Y, Z). Each column becomes one polygon; the WebGL
% renderer fan-triangulates and Lambert-shades each.
import compat.*;

clear
close all

% 4 triangle faces. Each column = (apex, base_left, base_right).
% Apex at (0, 0, 1.5); base square corners (±1, ±1, 0).
X = [ 0    0    0    0;
      1    1   -1   -1;
      1   -1   -1    1];
Y = [ 0    0    0    0;
     -1    1    1   -1;
      1    1   -1   -1];
Z = [1.5  1.5  1.5  1.5;
     0    0    0    0;
     0    0    0    0];

figure
fill3(X, Y, Z, '#ff8c42')
title('fill3 — Pyramid')
xlabel('X')
ylabel('Y')
zlabel('Z')
grid on
view(45, 25)
