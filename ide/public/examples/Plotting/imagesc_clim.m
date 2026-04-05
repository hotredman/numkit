% Color Limits — imagesc with clim
% Use clim (or caxis) to control the color range,
% clamping values outside the limits.
clear
close all

% Peaks-like surface
n = 50;
Z = zeros(n, n);
for i = 1:n
    for j = 1:n
        x = -3 + 6 * (j-1) / (n-1);
        y = -3 + 6 * (i-1) / (n-1);
        Z(i,j) = 3*(1-x)^2 * exp(-x^2 - (y+1)^2) ...
                - 10*(x/5 - x^3 - y^5) * exp(-x^2 - y^2) ...
                - 1/3 * exp(-(x+1)^2 - y^2);
    end
end

figure(1)
imagesc(Z)
colormap('turbo')
title('Full range')

figure(2)
imagesc(Z, [-3 3])
colormap('turbo')
title('clim [-3, 3]')
