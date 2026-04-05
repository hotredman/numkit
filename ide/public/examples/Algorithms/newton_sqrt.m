% Newton Sqrt — Square root via Newton's method
% Iterative approximation converging to sqrt(x).
clear

function r = newton_sqrt(x)
    r = x;
    for i = 1:20
        r = (r + x/r) / 2;
    end
end

disp(newton_sqrt(2))
disp(newton_sqrt(144))
