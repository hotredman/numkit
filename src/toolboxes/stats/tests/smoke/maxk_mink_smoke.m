clear

x = [3 1 4 1 5 9 2 6];

fprintf('=== maxk / mink basic ===\n');
disp(maxk(x, 3));
disp(mink(x, 3));

fprintf('\n=== ComparisonMethod ===\n');
disp(maxk(x, 3, 'ComparisonMethod', 'real'));
disp(maxk(x, 3, 'ComparisonMethod', 'auto'));
try
    maxk(x, 3, 'ComparisonMethod', 'abs');
catch err
    fprintf('  abs: %s\n', err.message);
end
