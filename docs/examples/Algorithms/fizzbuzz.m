% FizzBuzz — Classic programming challenge
% Print Fizz for multiples of 3, Buzz for 5, FizzBuzz for both.
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
