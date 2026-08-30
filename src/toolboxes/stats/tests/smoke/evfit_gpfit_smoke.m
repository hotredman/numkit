clear

rng(0);
x = evrnd(1.0, 2.0, 3000, 1);
p = evfit(x);
fprintf('evfit: mu_hat=%.4f, sigma_hat=%.4f  (true mu=1.0, sigma=2.0)\n', p(1), p(2));
fprintf('       expect ~|err_mu| < 0.15, |err_sigma| < 0.2\n');

rng(1);
y = gprnd(0.3, 1.5, 0, 3000, 1);
q = gpfit(y);
fprintf('gpfit: k_hat=%.4f, sigma_hat=%.4f  (true k=0.3, sigma=1.5)\n', q(1), q(2));
fprintf('       expect ~|err_k| < 0.15, |err_sigma| < 0.3 (PWM estimator)\n');

% Exponential limit: k=0 reduces GP to Exp.
rng(2);
z = exprnd(1.0, 5000, 1);
r = gpfit(z);
fprintf('gpfit exponential limit: k_hat=%.4f (expect ~0), sigma_hat=%.4f (expect ~1.0)\n', r(1), r(2));
