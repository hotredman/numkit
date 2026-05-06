clear

import compat.*

% orthfilt — quadruple {Lo_D, Hi_D, Lo_R, Hi_R} from a unit-norm
% scaling filter (sum(W) = 1).

[Lo_D, Hi_D, Lo_R, Hi_R] = orthfilt(dbwavf('db2'));
fprintf('Lo_D: '); disp(Lo_D);
fprintf('  expect: [-0.1294 0.2241 0.8365 0.4830]\n\n');
fprintf('Hi_D: '); disp(Hi_D);
fprintf('  expect: [-0.4830 0.8365 -0.2241 -0.1294]\n\n');
fprintf('Lo_R: '); disp(Lo_R);
fprintf('  expect: [0.4830 0.8365 0.2241 -0.1294]\n\n');
fprintf('Hi_R: '); disp(Hi_R);
fprintf('  expect: [-0.1294 -0.2241 0.8365 -0.4830]\n');
