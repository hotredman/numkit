% Histogram — Distribution of random data
% Generate normally-distributed samples and visualize with hist().
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear
close all

data = randn(1, 500);
figure
hist(data, 25)
title('Normal Distribution (500 samples)')
xlabel('Value')
ylabel('Count')
