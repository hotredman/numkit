% Arithmetic — Basic math operations
% Demonstrates operator precedence, parentheses, power, and modulo.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

disp(2 + 3 * 4 - 1)
disp((2 + 3) * (4 - 1))
disp(2 ^ 3 ^ 2)
disp(mod(17, 5))
