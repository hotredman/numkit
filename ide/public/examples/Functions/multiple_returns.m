% Multiple Returns — Function returning min and max
% A function that returns two output values.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

function [mn, mx] = minmax(v)
    mn = v(1); mx = v(1);
    for i = 2:length(v)
        if v(i) < mn
            mn = v(i);
        end
        if v(i) > mx
            mx = v(i);
        end
    end
end

[lo, hi] = minmax([5 3 9 1 7]);
disp(lo)
disp(hi)
