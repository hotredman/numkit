% Image Display — imagesc with colormap
% Display a matrix as a color-scaled image.
% Use colormap() to switch between palettes.
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
