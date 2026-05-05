% Deconvolution — Polynomial division with deconv
% If c = conv(a, b), then deconv(c, a) recovers b.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

a = [1 2 3];
b = [4 5];

c = conv(a, b);
disp('conv(a, b):')
disp(c)

[q, r] = deconv(c, a);
disp('Recovered quotient (should be [4 5]):')
disp(q)
disp('Remainder (should be ~0):')
disp(r)
