clear

rng(0);

fprintf('=== gamfit ===\n');
fprintf('True (a, b) = (2.0, 3.0):\n');
fit_g = gamfit(gamrnd(2.0, 3.0, 2000, 1));
fprintf('  fit = [%.4f, %.4f]  (recovered)\n', fit_g(1), fit_g(2));

fprintf('\nTrue (a, b) = (0.5, 1.0)  (heavy-tail):\n');
fit_g2 = gamfit(gamrnd(0.5, 1.0, 5000, 1));
fprintf('  fit = [%.4f, %.4f]\n', fit_g2(1), fit_g2(2));

fprintf('\n=== wblfit ===\n');
fprintf('True (a, b) = (3.0, 2.0):\n');
fit_w = wblfit(wblrnd(3.0, 2.0, 2000, 1));
fprintf('  fit = [%.4f, %.4f]\n', fit_w(1), fit_w(2));

fprintf('\nTrue (a, b) = (1.0, 0.7)  (long-tailed):\n');
fit_w2 = wblfit(wblrnd(1.0, 0.7, 5000, 1));
fprintf('  fit = [%.4f, %.4f]\n', fit_w2(1), fit_w2(2));
