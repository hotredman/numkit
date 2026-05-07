clear

import compat.*

ps = [0.05 0.5 0.95];

fprintf('=== finv ===\n');

x = finv(ps, 1, 1);
fprintf('  F(1,1)   : [%.4f %.4f %.4f] (expect [0.0062 1.0000 161.4476])\n', x(1), x(2), x(3));

x = finv(ps, 5, 10);
fprintf('  F(5,10)  : [%.4f %.4f %.4f] (expect [0.2112 0.9319 3.3258])\n', x(1), x(2), x(3));

x = finv(ps, 10, 30);
fprintf('  F(10,30) : [%.4f %.4f %.4f] (expect [0.3704 0.9554 2.1646])\n', x(1), x(2), x(3));

fprintf('\n--- edges ---\n');
fprintf('  p=0    : %g (expect 0)\n', finv(0.0, 5, 10));
fprintf('  p=1    : %g (expect Inf)\n', finv(1.0, 5, 10));
fprintf('  p<0    : %g (expect NaN)\n', finv(-0.1, 5, 10));
fprintf('  p>1    : %g (expect NaN)\n', finv(1.5, 5, 10));
fprintf('  v1=0   : %g (expect NaN)\n', finv(0.5, 0, 10));
fprintf('  v2=0   : %g (expect NaN)\n', finv(0.5, 5, 0));
fprintf('  v1<0   : %g (expect NaN)\n', finv(0.5, -1, 10));
