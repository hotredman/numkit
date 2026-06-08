clear
import compat.*

% convenc — convolutional encoder driven by a poly2trellis trellis.
% Error Correction Codes section of the Communications Toolbox.

t = poly2trellis(3, [6 7]);   % rate 1/2, K=3

code = convenc([1 1 0 1 1 0 0], t);
fprintf('convenc([1 1 0 1 1 0 0]) =\n');
disp(code);
fprintf('  expect [1 1 0 0 1 0 1 0 0 0 1 0 0 1] (numel %d = 14, sum %d = 6)\n', ...
        numel(code), sum(code));

c2 = convenc([1 0 0], t);
fprintf('convenc([1 0 0]) = ');
disp(c2);
fprintf('  expect [1 1 1 1 0 1]\n');

% Rate 1/3, K=4: 4 input bits -> 12 output bits.
t3 = poly2trellis(4, [13 15 17]);
c3 = convenc([1 0 1 1], t3);
fprintf('rate-1/3 convenc([1 0 1 1]) numel = %d  (expect 12)\n', numel(c3));
