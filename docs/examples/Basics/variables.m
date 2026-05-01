% Variables — Assign and use variables
% Basic variable assignment, arithmetic with variables.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

x = 42;
y = 3.14;
result = x * y;
disp(result)
