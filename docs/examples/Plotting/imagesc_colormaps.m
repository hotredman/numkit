% Colormaps — Compare different color palettes
% Use subplot + colormap() to display the same data
% with jet, hot, parula, and gray colormaps.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear
close all

% Sinc-like pattern
n = 30;
Z = zeros(n, n);
for i = 1:n
    for j = 1:n
        r = sqrt((i - n/2)^2 + (j - n/2)^2) + 1;
        Z(i,j) = sin(r) / r;
    end
end

figure

subplot(2, 2, 1)
imagesc(Z)
colormap('jet')
title('jet')

subplot(2, 2, 2)
imagesc(Z)
colormap('hot')
title('hot')

subplot(2, 2, 3)
imagesc(Z)
colormap('parula')
title('parula')

subplot(2, 2, 4)
imagesc(Z)
colormap('gray')
title('gray')
