clear
import compat.*

% grpstats now returns its default multi-output set when no 'whichstats'
% argument is given: [means, sem, counts] = grpstats(X, group), matching
% MATLAB. numkit previously emitted only the means, so requesting the 2nd or
% 3rd output threw "Index 1 exceeds array size 0". sem = std/sqrt(n) is the
% standard error of the mean; counts is the number of rows per group.

x = [1 2 3 4 5 6]';
g = [1 1 2 2 3 3]';

fprintf('--- [means, sem, counts] = grpstats(x, g) ---\n');
[m, s, c] = grpstats(x, g);
fprintf('means : %.4f %.4f %.4f   (expect 1.5 3.5 5.5)\n', m(1), m(2), m(3));
fprintf('sem   : %.4f %.4f %.4f   (expect 0.5 0.5 0.5)\n', s(1), s(2), s(3));
fprintf('counts: %d %d %d   (expect 2 2 2)\n', c(1), c(2), c(3));

fprintf('--- multi-column X ---\n');
X = [1 10; 2 20; 3 30; 4 40; 5 50; 6 60];
[mm, ss, cc] = grpstats(X, g);
fprintf('means row1: %.4f %.4f   sem row1: %.4f %.4f   (expect 1.5 15, 0.5 5)\n', ...
        mm(1,1), mm(1,2), ss(1,1), ss(1,2));

fprintf('--- single-output and whichstats forms unchanged ---\n');
m1 = grpstats(x, g);
fprintf('1-out means: %.4f %.4f %.4f\n', m1(1), m1(2), m1(3));
sd = grpstats(x, g, 'std');
fprintf('whichstats ''std'': %.6f\n', sd(1));
