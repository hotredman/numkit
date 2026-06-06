% For Loop — Sum of 1 to 100
% Classic Gauss summation using a for loop.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

total = 0;
for i = 1:100
    total = total + i;
end
disp(total)
