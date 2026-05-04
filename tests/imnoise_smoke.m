import compat.*

rng(42);  % deterministic — exact stats match across runs

% Constant gray image; we measure the noise statistics of (J - I).
I = 0.5 * ones(200, 200);

% --- gaussian, default var=0.01 ---
J = imnoise(I, 'gaussian');
d = J - I;
fprintf('--- imnoise(I, ''gaussian''), default mean=0 var=0.01 ---\n');
fprintf('  mean(d)  = %.4f (expect ~0)\n', mean(d(:)));
fprintf('  var(d)   = %.4f (expect ~0.01)\n\n', var(d(:)));

% --- gaussian with explicit mean=0.05 var=0.005 ---
rng(42);
J2 = imnoise(I, 'gaussian', 0.05, 0.005);
d2 = J2 - I;
fprintf('--- gaussian, mean=0.05 var=0.005 ---\n');
fprintf('  mean(d) = %.4f (expect ~0.05)\n', mean(d2(:)));
fprintf('  var(d)  = %.4f (expect ~0.005)\n\n', var(d2(:)));

% --- salt & pepper, density=0.10 ---
rng(42);
K = imnoise(I, 'salt & pepper', 0.10);
n_salt   = sum(K(:) == 1);
n_pepper = sum(K(:) == 0);
fprintf('--- imnoise(I, ''salt & pepper'', 0.10) ---\n');
fprintf('  salt frac   = %.4f (expect ~0.05)\n', n_salt / numel(K));
fprintf('  pepper frac = %.4f (expect ~0.05)\n', n_pepper / numel(K));
fprintf('  total noisy = %.4f (expect ~0.10)\n\n', ...
    (n_salt + n_pepper) / numel(K));

% --- speckle on bright base — variance scales with intensity ---
% I=0.5 → variance ≈ 0.5^2 * 0.04 = 0.01
rng(42);
S = imnoise(I, 'speckle', 0.04);
ds = S - I;
fprintf('--- imnoise(I, ''speckle'', 0.04) on I=0.5 ---\n');
fprintf('  mean(d) = %.4f (expect ~0)\n', mean(ds(:)));
fprintf('  var(d)  = %.4f (expect ~0.5^2*0.04 = 0.01)\n\n', var(ds(:)));

% --- poisson on uint16 (count-like): variance ≈ mean ---
% I=200 (counts) → mean ≈ 200, var ≈ 200
I_u16 = uint16(200 * ones(300, 300));
rng(42);
P = imnoise(I_u16, 'poisson');
dp = double(P) - double(I_u16);
fprintf('--- imnoise(uint16, ''poisson'') on counts=200 ---\n');
fprintf('  mean(d) = %.4f (expect ~0)\n', mean(dp(:)));
fprintf('  var(d)  = %.4f (expect ~200 — Poisson Var=mean)\n\n', var(dp(:)));

% --- localvar: per-pixel variance map ---
% Variance = 0 in left half, 0.01 in right half.
V = zeros(100, 200);
V(:, 101:end) = 0.01;
I3 = 0.5 * ones(100, 200);
rng(42);
L = imnoise(I3, 'localvar', V);
left  = L(:, 1:100);
right = L(:, 101:end);
fprintf('--- imnoise(I, ''localvar'', V) — V=0|0.01 split ---\n');
fprintf('  var(left)  = %.6f (expect 0 — variance map zero)\n', var(left(:)));
fprintf('  var(right) = %.4f  (expect ~0.01)\n', var(right(:)));

% --- unknown mode ---
ok = false;
try
    imnoise(I, 'wat');
catch
    ok = true;
end
fprintf('  unknown-mode raises = %d (expect 1)\n', ok);
