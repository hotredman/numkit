clear
import compat.*

fprintf('=== lillietest (Lilliefors normality test) ===\n');

% Normal-ish small sample
x_norm = [0.1 0.5 -0.3 1.2 -0.7 0.4 -0.1 0.8 -0.4 0.3 0.6 -0.2 0.7 -0.5 0.1];
[h1, p1, ks1, cv1] = lillietest(x_norm);
fprintf('  Normal-like (n=15):\n');
fprintf('    h=%d (0=keep H0)  p=%g  KS=%g  crit=%g\n', h1, p1, ks1, cv1);

% Strongly bimodal -- should reject
x_bi = [-3 -3 -3 -3 -3 -3 -3 -3 -3 -3 3 3 3 3 3 3 3 3 3 3];
[h2, p2, ks2, cv2] = lillietest(x_bi);
fprintf('  Bimodal (n=20):\n');
fprintf('    h=%d (1=reject)  p=%g  KS=%g  crit=%g\n', h2, p2, ks2, cv2);

% Different alpha
x_lin = (1:30) / 31;   % uniform-ish
[h3, p3, ks3, cv3a] = lillietest(x_lin, 0.05);
[h3b, p3b, ks3b, cv3b] = lillietest(x_lin, 0.01);
fprintf('  Uniform (n=30) at alpha=0.05: h=%d crit=%g\n', h3, cv3a);
fprintf('  Uniform (n=30) at alpha=0.01: h=%d crit=%g (stricter -> larger crit)\n', h3b, cv3b);
