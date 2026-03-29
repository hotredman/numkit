% Statistical Analysis — Mean, variance, standard deviation
% Compute basic statistics from scratch using loops.
clear

data = [4 8 15 16 23 42];
n = length(data);
m = sum(data) / n;
disp(m)

v = 0;
for i = 1:n
    v = v + (data(i) - m)^2;
end
v = v / (n - 1);
disp(v)
disp(sqrt(v))
