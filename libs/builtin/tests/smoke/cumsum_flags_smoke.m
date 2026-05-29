clear
import compat.*
% cumsum/cumprod direction + nanflag (MATLAB R2025b).
rr = cumsum([1 2 3 4], 'reverse');
fprintf('cumsum reverse : %g %g %g %g (expect 10 9 7 4)\n', rr(1),rr(2),rr(3),rr(4));

pr = cumprod([1 2 3 4], 'reverse');
fprintf('cumprod reverse: %g %g %g %g (expect 24 24 12 4)\n', pr(1),pr(2),pr(3),pr(4));

so = cumsum([1 NaN 3], 'omitnan');
fprintf('cumsum omitnan : %g %g %g (expect 1 1 4)\n', so(1),so(2),so(3));

md = cumsum([1 2 3; 4 5 6], 2, 'reverse');
fprintf('dim2 reverse row1: %g %g %g (expect 6 5 3)\n', md(1,1),md(1,2),md(1,3));
