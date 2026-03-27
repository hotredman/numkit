% Sine Wave — Basic line plot
x = linspace(0, 2*pi, 100);
y = sin(x);
plot(x, y)
title('Sine Wave')
xlabel('x')
ylabel('sin(x)')
