clear

ps = [0.05 0.5 0.95];

fprintf('=== betainv ===\n');

x = betainv(ps, 1, 1);
fprintf('  Beta(1,1) [uniform]  : [%.4f %.4f %.4f] (expect [0.0500 0.5000 0.9500])\n', x(1), x(2), x(3));

x = betainv(ps, 0.5, 0.5);
fprintf('  Beta(0.5,0.5) [arc]  : [%.6f %.4f %.6f] (expect [0.006156 0.5000 0.993844])\n', x(1), x(2), x(3));

x = betainv(ps, 2, 5);
fprintf('  Beta(2,5) [skew]     : [%.4f %.4f %.4f] (expect [0.0628 0.2645 0.5818])\n', x(1), x(2), x(3));

x = betainv(ps, 10, 10);
fprintf('  Beta(10,10) [narrow] : [%.4f %.4f %.4f] (expect [0.3201 0.5000 0.6799])\n', x(1), x(2), x(3));

fprintf('\n--- edges ---\n');
fprintf('  p=0   : %g (expect 0)\n', betainv(0.0, 2, 3));
fprintf('  p=1   : %g (expect 1)\n', betainv(1.0, 2, 3));
fprintf('  p<0   : %g (expect NaN)\n', betainv(-0.1, 2, 3));
fprintf('  p>1   : %g (expect NaN)\n', betainv( 1.1, 2, 3));
fprintf('  a=0   : %g (expect NaN)\n', betainv(0.5, 0, 3));
