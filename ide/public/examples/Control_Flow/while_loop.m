% While Loop — First power of 2 >= 1000
% Repeatedly double until the threshold is reached.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

n = 1;
while n < 1000
    n = n * 2;
end
disp(n)
