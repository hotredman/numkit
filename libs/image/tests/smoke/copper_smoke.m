clear

import compat.*

% copper — black-to-copper colormap.

fprintf('--- size(copper()) ---\n');
c = copper();
fprintf('size: %s  (expect [256 3])\n', mat2str(size(c)));

fprintf('\n--- copper(5) ---\n');
disp(copper(5));
fprintf('  expect: r=min(5/4*x,1), g=0.7812*x, b=0.4975*x\n');

fprintf('\n--- copper(1) ---\n');
disp(copper(1));
fprintf('  expect [0 0 0]\n');

fprintf('\n--- copper(0) size ---\n');
fprintf('size copper(0)  = %s\n', mat2str(size(copper(0))));
