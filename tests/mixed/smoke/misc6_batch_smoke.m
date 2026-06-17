clear

import compat.*

% Misc batch 6 + signal batch — spec closure 2026-05-09.

rng(42); v = randi(10, 1, 5);
fprintf('randi(10,1,5) = '); disp(v);
p = randperm(5);
fprintf('randperm(5) = '); disp(p);
[x, y] = pol2cart(pi/4, sqrt(2));
fprintf('pol2cart(pi/4, sqrt(2)) = (%g, %g)\n', x, y);
[x, y, z] = sph2cart(0, 0, 1);
fprintf('sph2cart(0,0,1) = (%g, %g, %g)\n', x, y, z);
fprintf('typecast(uint32(0x3F800000), single) = %g\n', typecast(uint32(0x3F800000), "single"));
fprintf('split("a,b,c", ",") = '); disp(split("a,b,c", ","));
fprintf('strjoin({a,b,c}, ",") = "%s"\n', strjoin({'a','b','c'}, ','));
fprintf('strncmp(hello,help,3) = %d\n', strncmp('hello','help',3));

% Signal namespace
fprintf('\n=== signal batch ===\n');
fprintf('conv([1 2 3],[1 1]) = '); disp(conv([1 2 3],[1 1]));
fprintf('conv2([1 2;3 4],[1 1;1 1]):\n'); disp(conv2([1 2;3 4],[1 1;1 1]));
fprintf('chirp(0:0.001:0.01, 0, 1, 100)(1) = %g\n', chirp(0:0.001:0.01, 0, 1, 100)(1));
[z, p, k] = buttap(3);
fprintf('buttap(3): %d poles, k=%g\n', numel(p), k);
