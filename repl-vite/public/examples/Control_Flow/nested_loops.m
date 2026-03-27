% Nested Loops — Multiplication table
for i = 1:5
    row = '';
    for j = 1:5
        row = [row, num2str(i*j), ' '];
    end
    disp(row)
end
