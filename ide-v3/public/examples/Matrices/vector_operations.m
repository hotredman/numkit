% Vector Operations — Ranges, sum, min, max
% Create a range vector and apply aggregation functions.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

v = 1:10;
disp(v)
disp(sum(v))
disp(min(v))
disp(max(v))
