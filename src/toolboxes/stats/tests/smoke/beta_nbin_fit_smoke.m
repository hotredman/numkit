clear

rng(0);

fprintf('=== betafit ===\n');
% Variety of true parameters
for ab = [2 5; 0.5 0.5; 10 2; 1 1]'
    a = ab(1); b = ab(2);
    fit = betafit(betarnd(a, b, 3000, 1));
    fprintf('  true=(%4.2f, %4.2f)  fit=(%.4f, %.4f)\n', a, b, fit(1), fit(2));
end

fprintf('\n=== nbinfit ===\n');
for rp = [3 0.4; 1 0.5; 10 0.3]'
    r = rp(1); p = rp(2);
    fit = nbinfit(nbinrnd(r, p, 3000, 1));
    fprintf('  true=(%4.2f, %4.2f)  fit=(%.4f, %.4f)\n', r, p, fit(1), fit(2));
end
