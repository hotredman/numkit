clear

% fftconv2 — FFT-based 2-D convolution; matches conv2 within FP eps.

% --- tiny ---
fprintf('--- 2x2 * 2x2 ---\n');
A = [1 2; 3 4];
B = [5 6; 7 8];
ff = real(fftconv2(A, B));
cc = conv2(A, B);
fprintf('max|err| = %.4e\n', max(abs(ff(:) - cc(:))));

% --- larger non-pow2 sizes (5x10 * 7x8 → 11x17) ---
fprintf('\n--- 5x10 * 7x8 (full) ---\n');
a = repmat(1:10, 5, 1);
b = repmat(10:-1:3, 7, 1);
ff = real(fftconv2(a, b));
cc = conv2(a, b);
fprintf('size %s, max|err| = %.4e\n', mat2str(size(ff)), ...
        max(abs(ff(:) - cc(:))));

% --- same shape ---
fprintf('\n--- "same" ---\n');
fs = real(fftconv2(a, b, 'same'));
cs = conv2(a, b, 'same');
fprintf('size %s, max|err| = %.4e\n', mat2str(size(fs)), ...
        max(abs(fs(:) - cs(:))));

% --- valid: b is wider, full conv has no fully-overlapping region ---
fprintf('\n--- "valid" b vs a (one axis empty) ---\n');
fv = fftconv2(a, b, 'valid');
fprintf('size %s\n', mat2str(size(fv)));

% --- valid with non-empty result: square inputs ---
fprintf('\n--- "valid" 8x8 * 3x3 ---\n');
A2 = magic_like(8);
B2 = ones(3);
fv2 = real(fftconv2(A2, B2, 'valid'));
cv2 = conv2(A2, B2, 'valid');
fprintf('size %s, max|err| = %.4e\n', mat2str(size(fv2)), ...
        max(abs(fv2(:) - cv2(:))));

function M = magic_like(n)
    M = mod((1:n)' + (1:n) - 1, n) + 1;
end
