clear

import compat.*

% imflatfield — flat-field correction by Gaussian-blur normalization.

% --- 2-D: synthetic shading on uniform image ---
fprintf('--- imflatfield (2-D, σ=20) ---\n');
[XX, YY] = meshgrid(1:50, 1:50);
shading = 0.5 + 0.5 * (XX + YY) / 100;       % smooth gradient in [0.5, 1.5]
I0      = 0.6 * ones(50, 50);                % uniform target
A       = I0 .* shading;                     % observed = target × shading
B       = imflatfield(A, 20);
fprintf('input  range: [%.4f, %.4f]\n', min(A(:)), max(A(:)));
fprintf('output range: [%.4f, %.4f]\n', min(B(:)), max(B(:)));
fprintf('output std/mean: %.4e (expect smaller than input %.4e)\n', ...
        std(B(:))/mean(B(:)), std(A(:))/mean(A(:)));
fprintf('  expect: shading flattened, std/mean reduced\n\n');

% --- 3-D RGB ---
fprintf('--- imflatfield (3-D RGB, σ=10) ---\n');
R       = 0.5 * ones(20, 20) .* shading(1:20, 1:20);
G       = 0.7 * ones(20, 20) .* shading(1:20, 1:20);
Bch     = 0.3 * ones(20, 20) .* shading(1:20, 1:20);
RGB     = cat(3, R, G, Bch);
RGBflat = imflatfield(RGB, 10);
fprintf('size: %s\n', mat2str(size(RGBflat)));
fprintf('per-channel mean: in=[%.3f %.3f %.3f] out=[%.3f %.3f %.3f]\n', ...
        mean(R(:)), mean(G(:)), mean(Bch(:)), ...
        mean(reshape(RGBflat(:,:,1),1,[])), ...
        mean(reshape(RGBflat(:,:,2),1,[])), ...
        mean(reshape(RGBflat(:,:,3),1,[])));
fprintf('  expect: per-channel mean preserved\n\n');

% --- mask: restrict mean estimate to a region ---
fprintf('--- imflatfield with mask ---\n');
M           = false(50, 50);
M(10:40, 10:40) = true;
Bm          = imflatfield(A, 20, M);
fprintf('output range: [%.4f, %.4f]\n', min(Bm(:)), max(Bm(:)));
fprintf('  expect: similar to no-mask result on this synthetic input\n');
