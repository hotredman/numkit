% Newton Sqrt — Square root via Newton's method
% Iterative approximation converging to sqrt(x).
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

function r = newton_sqrt(x)
    r = x;
    for i = 1:20
        r = (r + x/r) / 2;
    end
end

disp(newton_sqrt(2))
disp(newton_sqrt(144))
