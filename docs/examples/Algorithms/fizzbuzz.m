% FizzBuzz — Classic challenge
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
