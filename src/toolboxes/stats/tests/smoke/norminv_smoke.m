clear

ps = [0.025 0.5 0.975];

fprintf('=== norminv ===\n');

x = norminv(ps);
fprintf('  default N(0,1) : [%.4f %.4f %.4f] (expect [-1.9600 0.0000 1.9600])\n', x(1), x(2), x(3));

x = norminv(ps, 5, 1);
fprintf('  mu=5, sig=1    : [%.4f %.4f %.4f] (expect [3.0400 5.0000 6.9600])\n', x(1), x(2), x(3));

x = norminv(ps, 0, 2);
fprintf('  mu=0, sig=2    : [%.4f %.4f %.4f] (expect [-3.9199 0.0000 3.9199])\n', x(1), x(2), x(3));

fprintf('\n--- edges ---\n');
fprintf('  p=0   : %g (expect -Inf)\n', norminv(0));
fprintf('  p=1   : %g (expect Inf)\n', norminv(1));
fprintf('  p<0   : %g (expect NaN)\n', norminv(-0.1));
fprintf('  p>1   : %g (expect NaN)\n', norminv( 1.5));
fprintf('  sig=0 : %g (expect NaN)\n', norminv(0.5, 0, 0));
fprintf('  sig<0 : %g (expect NaN)\n', norminv(0.5, 0, -1));
