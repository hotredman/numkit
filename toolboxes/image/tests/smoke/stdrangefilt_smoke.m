clear

import compat.*

% stdfilt + rangefilt — sliding-window local statistics.

fprintf('--- stdfilt(ones(5)) (uniform → all zeros) ---\n');
disp(stdfilt(ones(5)));

fprintf('\n--- stdfilt([1 1 1; 2 2 2; 3 3 3]) ---\n');
C = [1 1 1; 2 2 2; 3 3 3];
disp(stdfilt(C));
fprintf('  expect: each column same; rows have std of 9 elems via symmetric pad\n');

fprintf('\n--- rangefilt(C) ---\n');
disp(rangefilt(C));
fprintf('  expect: max-min over 3x3 nbhd, symmetric pad\n');

fprintf('\n--- stdfilt with custom 1x3 domain ---\n');
A = [1 2 3 4 5];
disp(stdfilt(A, [1 1 1]));
fprintf('  expect: sample std of [1 1 2], [1 2 3], [2 3 4], [3 4 5], [4 5 5]\n');

fprintf('\n--- rangefilt: 0 if uniform, else max-min ---\n');
B = [1 2 3 4; 5 6 7 8];
disp(double(rangefilt(B)));
