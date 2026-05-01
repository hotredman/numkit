% Math Functions — sqrt, abs, log, exp, rounding
% Common mathematical functions available in MATLAB.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

disp(sqrt(144))
disp(abs(-7))
disp(log(exp(5)))
disp(floor(3.7))
disp(ceil(-2.3))
