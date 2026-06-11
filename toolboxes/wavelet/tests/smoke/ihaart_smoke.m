clear

import compat.*

[a, d] = haart([1 2 3 4 5 6 7 8]);
xr = ihaart(a, d);
fprintf('full reconstruction: '); disp(xr');

xr1 = ihaart(a, d, 1);
fprintf('zero-out level=1: '); disp(xr1');
fprintf('  expect: [1.5 1.5 3.5 3.5 5.5 5.5 7.5 7.5]\n');
