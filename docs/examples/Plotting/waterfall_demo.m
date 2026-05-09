% waterfall — Stacked row ribbons
% Each row of Z becomes a ribbon whose top edge follows Z(r,:) and
% bottom edge sits at z=0. Useful for visualising spectrograms or
% successive waveform snapshots stacked over time.
import compat.*;

clear
close all

% Damped travelling sinusoid: each row is a snapshot at a later time.
n = 10;
m = 30;
Z = zeros(n, m);
x = linspace(0, 4*pi, m);
for i = 1:n
    Z(i, :) = exp(-i / 10) * sin(x - i / 2);
end

figure
waterfall(Z)
title('Waterfall — Damped Travelling Wave')
xlabel('Position')
ylabel('Time step')
zlabel('Amplitude')
grid on
