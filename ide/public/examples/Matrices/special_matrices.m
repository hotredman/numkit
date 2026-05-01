% Special Matrices — eye, zeros, ones, linspace
% Built-in matrix constructors for common patterns.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

disp(eye(3))
disp(zeros(2))
disp(ones(1, 4))
disp(linspace(0, 1, 5))
