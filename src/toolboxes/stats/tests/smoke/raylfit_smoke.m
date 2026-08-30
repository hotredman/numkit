clear

x = [1.5 2.3 0.8 3.1 1.7 2.0 1.4 2.8 1.9 2.2]';

fprintf('=== raylfit ===\n');
[s, sc] = raylfit(x);
fprintf('  basic    : s=%.4f  ci=[%.4f, %.4f]\n', s, sc(1), sc(2));
fprintf('             (expect 1.4651 [1.1209, 2.1157])\n');

[s, sc] = raylfit(x, 0.01);
fprintf('  α=0.01   : ci=[%.4f, %.4f]\n', sc(1), sc(2));
fprintf('             (expect [1.0360, 2.4031])\n');

[s, sc] = raylfit([2.5]');
fprintf('  one-pt   : s=%.4f  ci=[%.4f, %.4f]\n', s, sc(1), sc(2));
fprintf('             (expect 1.7678 [0.9204, 11.1099])\n');

[s, sc] = raylfit([]);
fprintf('  empty    : s=%g  ci=[%g, %g]  (NaN)\n', s, sc(1), sc(2));
