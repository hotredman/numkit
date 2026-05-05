% If / Elseif / Else — Conditional branching
% Classify a value into size categories.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

val = 42;
if val > 100
    disp('big')
elseif val > 10
    disp('medium')
else
    disp('small')
end
