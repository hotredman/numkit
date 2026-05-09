% view — Change the camera angle
% view(2)         — top-down (az=0, el=90), looks like a 2-D heatmap
% view(3)         — default 3-D (-37.5°, 30°)
% view(az, el)    — explicit azimuth + elevation in degrees
%
% The figure built below is identical; the comment lines show what
% each call would do. Change which view() is uncommented to compare.
import compat.*;

clear
close all

n = 25;
x = linspace(-2, 2, n);
y = linspace(-2, 2, n);
Z = zeros(n, n);
for i = 1:n
    for j = 1:n
        Z(i, j) = sin(x(j)) * cos(y(i));
    end
end

figure
surf(x, y, Z)
title('view(45, 30)')
xlabel('X')
ylabel('Y')
zlabel('Z')
grid on

% Pick a camera angle:
view(45, 30)
% view(2)            % top-down
% view(3)            % default 3-D
% view(0, 0)         % looking straight along +Y
% view(90, 0)        % looking straight along -X
