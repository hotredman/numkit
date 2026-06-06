% Image Display — imagesc with colormap
% Display a matrix as a color-scaled image.
% Use colormap() to switch between palettes.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear
close all

% Generate a 2D Gaussian
x = linspace(-2, 2, 40);
y = linspace(-2, 2, 40);
Z = zeros(40, 40);
for i = 1:40
    for j = 1:40
        Z(i,j) = exp(-(x(j)^2 + y(i)^2));
    end
end

figure
imagesc(x, y, Z)
title('2D Gaussian')
xlabel('x')
ylabel('y')
