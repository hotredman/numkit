import compat.*

% --- wthresh hard / soft ---
X = [-3 -2 -1 0 1 2 3];
fprintf('--- wthresh hard, T=1.5 ---\n');
disp(wthresh(X, 'h', 1.5));
fprintf('  expect: [-3 -2 0 0 0 2 3]\n\n');

fprintf('--- wthresh soft, T=1.5 ---\n');
disp(wthresh(X, 's', 1.5));
fprintf('  expect: [-1.5 -0.5 0 0 0 0.5 1.5]\n\n');

% --- wnoisest on a synthetic noisy signal ---
% pure WGN: σ should equal ~ 1.0
rng(42);
n = randn(1, 1024);
[C, L] = wavedec(n, 4, 'sym4');
sigmas = wnoisest(C, L, [1 2 3 4]);
fprintf('--- wnoisest on randn(1,1024), levels 1..4 ---\n');
disp(sigmas);
fprintf('  expect: all ~ 1.0\n\n');

% --- wdenoise on a smooth signal + noise ---
t = linspace(0, 1, 256);
clean = sin(2*pi*4*t) + 0.5*cos(2*pi*9*t);
rng(7);
noise = 0.3 * randn(size(clean));
noisy = clean + noise;

denoised = wdenoise(noisy, 4, 'sym4');

err_in  = sqrt(mean((noisy    - clean).^2));
err_out = sqrt(mean((denoised - clean).^2));

fprintf('--- wdenoise(noisy_signal, level=4, sym4) ---\n');
fprintf('  RMSE before denoise = %.4f\n', err_in);
fprintf('  RMSE after  denoise = %.4f\n', err_out);
fprintf('  expect: after << before (denoise reduces noise)\n');

% --- Default args (no level / no wname) ---
denoised2 = wdenoise(noisy);
err_default = sqrt(mean((denoised2 - clean).^2));
fprintf('  RMSE with defaults  = %.4f\n', err_default);
