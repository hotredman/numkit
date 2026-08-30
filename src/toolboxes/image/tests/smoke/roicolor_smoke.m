clear

% roicolor — region-of-interest mask by color (range or set).

fprintf('--- range form: roicolor([1:10], 2, 4) ---\n');
disp(double(roicolor([1 2 3 4 5 6 7 8 9 10], 2, 4)));
fprintf('  expect: [0 1 1 1 0 0 0 0 0 0]\n\n');

fprintf('--- range scalar: roicolor([1,2;3,4], 3, 3) ---\n');
disp(double(roicolor([1 2; 3 4], 3, 3)));
fprintf('  expect: [0 0; 1 0]\n\n');

fprintf('--- set form: roicolor([1,2;3,4], [1, 4]) ---\n');
disp(double(roicolor([1 2; 3 4], [1 4])));
fprintf('  expect: [1 0; 0 1]\n\n');

fprintf('--- set: roicolor(uint8([10 20 30; 30 40 30]), [20 30]) ---\n');
disp(double(roicolor(uint8([10 20 30; 30 40 30]), [20 30])));
