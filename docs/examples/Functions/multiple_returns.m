% Multiple Returns — Function returning min and max
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
