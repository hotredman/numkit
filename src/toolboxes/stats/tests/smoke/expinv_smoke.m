clear

ps = [0.05 0.5 0.95];

fprintf('=== expinv ===\n');
x = expinv(ps);
fprintf('  default mu=1   : [%.4f %.4f %.4f] (expect [0.0513 0.6931 2.9957])\n', x(1), x(2), x(3));

x = expinv(ps, 2);
fprintf('  mu=2           : [%.4f %.4f %.4f] (expect [0.1026 1.3863 5.9915])\n', x(1), x(2), x(3));

fprintf('  scalar (0.5,5) : %.4f (expect 3.4657)\n', expinv(0.5, 5));

fprintf('\n--- edges ---\n');
fprintf('  p=0   : %g (expect 0)\n', expinv(0.0));
fprintf('  p=1   : %g (expect Inf)\n', expinv(1.0));
fprintf('  p<0   : %g (expect NaN)\n', expinv(-0.1));
fprintf('  p>1   : %g (expect NaN)\n', expinv( 1.5));
fprintf('  mu=0  : %g (expect NaN)\n', expinv(0.5, 0));
fprintf('  mu<0  : %g (expect NaN)\n', expinv(0.5, -1));
