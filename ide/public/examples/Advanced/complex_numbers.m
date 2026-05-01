% Complex Numbers — Complex arithmetic
% Create complex values with the imaginary unit i or j,
% and extract magnitude, real, and imaginary parts.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

z1 = 3 + 4i;
z2 = 1 - 2i;

disp(z1 + z2)
disp(z1 * z2)
disp(abs(z1))
disp(real(z1))
disp(imag(z1))
