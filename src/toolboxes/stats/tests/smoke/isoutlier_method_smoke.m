clear

% isoutlier(A, method [,'ThresholdFactor',tf]) — DEEP-PROBE 2026-05-31.
% Bug: the method arg was parsed but IGNORED, so isoutlier(x,'mean')
% wrongly used the default median/MAD rule. Now each documented method
% maps to its own detector, operating per-column for matrices.
% Reference: MATLAB R2025b.

x = [1 2 3 100 4 5];

fprintf('=== median (default) ===\n');
m = isoutlier(x);
fprintf('m   = [%g %g %g %g %g %g]   (expect [0 0 0 1 0 0], sum=1)\n', ...
        m(1),m(2),m(3),m(4),m(5),m(6));

fprintf('\n=== median, ThresholdFactor 1 ===\n');
mt = isoutlier(x, 'median', 'ThresholdFactor', 1);
fprintf('mt  = [%g %g %g %g %g %g]   (expect [1 0 0 1 0 0], sum=2)\n', ...
        mt(1),mt(2),mt(3),mt(4),mt(5),mt(6));

fprintf('\n=== mean (3*std from full-sample mean) ===\n');
me = isoutlier(x, 'mean');
fprintf('me  = [%g %g %g %g %g %g]   (expect all 0 — 100 within 3*std)\n', ...
        me(1),me(2),me(3),me(4),me(5),me(6));

fprintf('\n=== mean, ThresholdFactor 1 ===\n');
me1 = isoutlier(x, 'mean', 'ThresholdFactor', 1);
fprintf('me1 = [%g %g %g %g %g %g]   (expect [0 0 0 1 0 0], sum=1)\n', ...
        me1(1),me1(2),me1(3),me1(4),me1(5),me1(6));

fprintf('\n=== quartiles (Q1-1.5IQR .. Q3+1.5IQR) ===\n');
q = isoutlier(x, 'quartiles');
fprintf('q   = [%g %g %g %g %g %g]   (expect [0 0 0 1 0 0], sum=1)\n', ...
        q(1),q(2),q(3),q(4),q(5),q(6));

fprintf('\n=== matrix (per column) ===\n');
M = isoutlier([1 2;3 4;5 100;7 8;9 10]);
fprintf('M(3,2) = %g  (expect 1)   sum(M(:)) = %g  (expect 1)\n', ...
        M(3,2), sum(double(M(:))));

fprintf('\n=== movmedian / movmean (moving window) ===\n');
xm = [1 2 3 100 5 6 7 8];
mm = isoutlier(xm, 'movmedian', 3);
fprintf('movmedian k=3 sum = %g  (expect 1, flags the 100)\n', sum(double(mm)));
ma = isoutlier(xm, 'movmean', 3);
fprintf('movmean   k=3 sum = %g  (expect 0 at default tf=3)\n', sum(double(ma)));
ma1 = isoutlier(xm, 'movmean', 3, 'ThresholdFactor', 1);
fprintf('movmean k=3 tf=1  = %g  (expect 1 at x(4))\n', double(ma1(4)));
y = [10 11 12 13 30 14 15 40 16 17];
my = isoutlier(y, 'movmedian', 5);
fprintf('movmedian y k=5 sum = %g  (expect 2: positions 5 and 8)\n', sum(double(my)));
