% Histogram — Distribution of random data
% Generate normally-distributed samples and visualize with hist().
clear
close all

data = randn(1, 500);
figure
hist(data, 25)
title('Normal Distribution (500 samples)')
xlabel('Value')
ylabel('Count')
