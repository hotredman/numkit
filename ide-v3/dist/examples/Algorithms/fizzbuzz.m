% FizzBuzz — Classic programming challenge
% Print Fizz for multiples of 3, Buzz for 5, FizzBuzz for both.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

for i = 1:20
    if mod(i, 15) == 0
        disp('FizzBuzz')
    elseif mod(i, 3) == 0
        disp('Fizz')
    elseif mod(i, 5) == 0
        disp('Buzz')
    else
        disp(i)
    end
end
