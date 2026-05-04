import compat.*

% --- Round-trip on randn (any length, no divisibility constraint) ---
rng(42);
x = randn(1, 65);    % deliberately not pow2 — MODWT works for any N
mwc = modwt(x, 3, 'db2');
fprintf('--- modwt(randn(1,65), 3, db2) ---\n');
fprintf('  size(mwc) = %dx%d (expect 4x65)\n', size(mwc, 1), size(mwc, 2));

xr = imodwt(mwc, 'db2');
fprintf('  max|x - xr| = %.6e (expect ≤ 1e-12)\n\n', max(abs(x - xr)));

% --- Energy preservation (Parseval): ||x||² == sum_{j,t} W_{j,t}² + sum_t V_{n,t}² ---
e_in = sum(x.^2);
e_out = sum(sum(mwc.^2));
fprintf('--- Energy preservation (Parseval) ---\n');
fprintf('  ||x||²    = %.6f\n', e_in);
fprintf('  ||mwc||²  = %.6f\n', e_out);
fprintf('  ratio     = %.6f (expect 1.0 to ~ 1e-13)\n\n', e_out / e_in);

% --- haar 4-level on a longer signal ---
rng(11);
y = randn(1, 200);
mwc4 = modwt(y, 4, 'haar');
yr = imodwt(mwc4, 'haar');
fprintf('--- modwt haar 4-level on randn(1,200): max round-trip err = %.6e ---\n', ...
    max(abs(y - yr)));

% --- sym4 on small signal ---
rng(7);
z = randn(1, 31);
zr = imodwt(modwt(z, 3, 'sym4'), 'sym4');
fprintf('--- modwt sym4 3-level on randn(1,31): max round-trip err = %.6e ---\n', ...
    max(abs(z - zr)));

% --- Shift-invariance — input right-shift ⇒ output right-shift ---
x = randn(1, 32);
x_sh = [x(end), x(1:end-1)];   % circular shift RIGHT by 1
m1 = modwt(x, 2, 'haar');
m2 = modwt(x_sh, 2, 'haar');
% MODWT(circshift(x,1)) should equal circshift(MODWT(x),1) along time.
% So m2 == circshift_cols(m1, +1) means m1(:,i) == m2(:,i+1), i.e.,
% reconstruct m1 from m2 by shifting LEFT by 1 (= right-shift undo).
m1_reconstructed_from_m2 = [m2(:, 2:end), m2(:, 1)];
fprintf('\n--- Shift-invariance check ---\n');
fprintf('  max|m1 - shift_left(m2)| = %.6e (expect ~ 0)\n', ...
    max(max(abs(m1 - m1_reconstructed_from_m2))));

% --- Constant signal: only V_n carries energy, all W_j ≈ 0 ---
c = 5 * ones(1, 64);
mc = modwt(c, 3, 'haar');
fprintf('\n--- modwt on constant=5 ---\n');
fprintf('  max|W_1..W_3| = %.6e (expect ~ 0, no high-freq content)\n', ...
    max(max(abs(mc(1:3, :)))));
fprintf('  V_3 mean = %.4f (expect 5.0)\n', mean(mc(4, :)));
