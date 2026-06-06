% Bar Chart — Simple bar chart
% Visualize a week of data as vertical bars.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear
close all

figure
bar([4 7 2 9 5 3 8])
title('Weekly Data')
xlabel('Day')
ylabel('Value')
