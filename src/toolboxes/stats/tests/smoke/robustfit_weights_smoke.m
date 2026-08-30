clear

% robustfit now ships ALL 9 MATLAB weight functions (was only bisquare/huber)
% and applies the DuMouchel-O'Brien leverage adjustment radj = r/sqrt(1-h),
% so coefficients are now bit-identical to MATLAB R2025b (previously ~2e-3 off
% on noisy data because leverage was omitted). The per-weight default tuning
% constants: andrews 1.339, bisquare 4.685, cauchy 2.385, fair 1.400,
% huber 1.345, logistic 1.205, ols 1, talwar 2.795, welsch 2.985.

x = (1:10)';
y = [1.1 1.9 3.2 3.9 5.1 6.0 12.0 8.1 9.0 9.9]';   % a line with one outlier

fprintf('weight     intercept     slope   (slope expect)\n');
ba  = robustfit(x, y, 'andrews');  fprintf('andrews  %10.6f %10.6f   (0.991377)\n', ba(1),  ba(2));
bsq = robustfit(x, y, 'bisquare'); fprintf('bisquare %10.6f %10.6f   (0.991382)\n', bsq(1), bsq(2));
bc  = robustfit(x, y, 'cauchy');   fprintf('cauchy   %10.6f %10.6f   (0.991703)\n', bc(1),  bc(2));
bf  = robustfit(x, y, 'fair');     fprintf('fair     %10.6f %10.6f   (0.998848)\n', bf(1),  bf(2));
bh  = robustfit(x, y, 'huber');    fprintf('huber    %10.6f %10.6f   (0.997666)\n', bh(1),  bh(2));
bl  = robustfit(x, y, 'logistic'); fprintf('logistic %10.6f %10.6f   (0.997075)\n', bl(1),  bl(2));
bt  = robustfit(x, y, 'talwar');   fprintf('talwar   %10.6f %10.6f   (0.991667)\n', bt(1),  bt(2));
bw  = robustfit(x, y, 'welsch');   fprintf('welsch   %10.6f %10.6f   (0.991318)\n', bw(1),  bw(2));
bo  = robustfit(x, y, 'ols');      fprintf('ols      %10.6f %10.6f   (1.082424 - outlier pulls it)\n', bo(1), bo(2));

fprintf('--- tuning override + intercept off ---\n');
bz = robustfit(x, y, 'huber', 1.0, 'off');
fprintf('huber tune=1 const=off -> single coeff = %.6f\n', bz(1));
