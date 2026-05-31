clear
import compat.*

U = [0.3 0.4; 0.5 0.6; 0.7 0.2; 0.1 0.9];
R = [1 0.5; 0.5 1];

fprintf('Gaussian copula:\n');
disp([copulapdf('Gaussian', U, R), copulacdf('Gaussian', U, R)]);
fprintf('  MATLAB pdf:  [1.1923; 1.1424; 0.7303; 0.2235]\n');
fprintf('  MATLAB cdf:  [0.1919; 0.3804; 0.1829; 0.0993]\n\n');

fprintf('Student-t copula (nu=5):\n');
disp([copulapdf('t', U, R, 5), copulacdf('t', U, R, 5)]);
fprintf('  MATLAB pdf:  [1.2908; 1.2457; 0.6723; 0.3268]\n');
fprintf('  MATLAB cdf:  [0.1927; 0.3801; 0.1780; 0.0969]\n\n');

fprintf('Clayton copula (alpha=2):\n');
disp([copulapdf('Clayton', U, 2), copulacdf('Clayton', U, 2)]);
fprintf('  MATLAB pdf:  [1.6034; 1.3847; 0.3159; 0.0409]\n');
fprintf('  MATLAB cdf:  [0.2472; 0.4160; 0.1960; 0.0999]\n\n');

fprintf('Frank copula (alpha=3):\n');
disp([copulapdf('Frank', U, 3), copulacdf('Frank', U, 3)]);
fprintf('  MATLAB pdf:  [1.2172; 1.1547; 0.6236; 0.2828]\n');
fprintf('  MATLAB cdf:  [0.1911; 0.3824; 0.1797; 0.0979]\n\n');

fprintf('Gumbel copula (alpha=1.5):\n');
disp([copulapdf('Gumbel', U, 1.5), copulacdf('Gumbel', U, 1.5)]);
fprintf('  MATLAB pdf:  [1.2371; 1.2000; 0.7278; 0.2828]\n');
fprintf('  MATLAB cdf:  [0.1844; 0.3825; 0.1792; 0.0985]\n');
