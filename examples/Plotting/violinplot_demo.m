% violinplot — KDE-shaped distribution + slim box + median dot
% Bandwidth uses Silverman's rule (1.06·σ·N^(-1/5)). The mirror shape
% lets you see bi-modality that boxplots hide; the inner box still
% gives the IQR for a quick numeric anchor.
import compat.*;

clear
close all

% Two groups: one unimodal Gaussian, one bi-modal.
g1 = randn(1, 100);
g2 = [randn(1, 50) - 1.5,  randn(1, 50) + 1.5];
M = [g1; g2]';

figure
violinplot(M)
title('Unimodal vs Bi-modal Distributions')
xlabel('Group')
ylabel('Value')
grid on
