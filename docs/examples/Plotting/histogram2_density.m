% histogram2 — 2-D histogram (density heatmap)
% Bins (X, Y) into an nx×ny grid of counts and renders the count
% matrix through the imagesc heatmap pipeline. Great for seeing
% structure in a scatter plot when the points overlap heavily.
import compat.*;

clear
close all

% 2 000 points around two foci.
N = 1000;
X = [randn(1, N) - 1, 2 * randn(1, N) + 2];
Y = [randn(1, N) + 1, 0.5 * randn(1, N) - 1];

figure
histogram2(X, Y, [40 30])
colorbar()
title('histogram2 — Two Gaussian Blobs')
xlabel('X')
ylabel('Y')
