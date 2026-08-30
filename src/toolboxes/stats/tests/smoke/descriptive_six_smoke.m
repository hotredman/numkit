clear

v = [1 5 3 8 2 7 4 6];
fprintf('=== range/mad/geomean/harmmean/moment/trimmean ===\n');
fprintf('  v = [1 5 3 8 2 7 4 6]\n');
fprintf('  range(v) = %g (expect 7)\n', range(v));
fprintf('  mad(v) = %g (mean abs dev, expect 2)\n', mad(v));
fprintf('  mad(v, 1) = %g (median abs dev, expect 2)\n', mad(v, 1));
fprintf('  geomean([2 4 8]) = %g (expect 4)\n', geomean([2 4 8]));
fprintf('  harmmean([1 2 4]) = %g (expect 12/7 = %g)\n', harmmean([1 2 4]), 12/7);
fprintf('  moment(v, 2) = %g (population variance)\n', moment(v, 2));
fprintf('  moment(v, 3) = %g (3rd central moment)\n', moment(v, 3));
fprintf('  trimmean(v, 25) = %g (mean trimming 12.5%% each end)\n', trimmean(v, 25));
fprintf('  trimmean(v, 0) = %g (= mean(v))\n', trimmean(v, 0));
