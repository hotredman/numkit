clear;

% imshow grayscale + RGB end-to-end.
% Expected output: figure markers per call; no errors.

% --- 1. grayscale double, default [0,1] range ---
I = [0 0.25 0.5; 0.5 0.75 1; 1 0.5 0];
imshow(I);
fprintf('imshow(I) OK (cmin=0 cmax=1 expected)\n');

% --- 2. grayscale double, explicit range ---
J = [10 20 30; 15 25 35; 12 22 32];
figure;
imshow(J, [10 35]);
fprintf('imshow(J, [10 35]) OK (cmin=10 cmax=35 expected)\n');

% --- 3. grayscale double, auto range ---
figure;
imshow(J, []);
fprintf('imshow(J, []) OK (cmin=10 cmax=35 from data extent)\n');

% --- 4. uint8 grayscale, range [0,255] ---
U = uint8([0 64 128; 192 255 128; 0 128 255]);
figure;
imshow(U);
fprintf('imshow(uint8) OK (cmin=0 cmax=255 expected)\n');

% --- 5. RGB double via cat(3,...) ---
R = [1 0; 0 1];
G = [0 1; 1 0];
B = [0.5 0.5; 0.5 0.5];
figure;
imshow(cat(3, R, G, B));
fprintf('imshow(RGB double) OK (image-rgb dataset, 2x2x3)\n');

% --- 6. logical mask ---
L = false(3, 3);
L(:,:) = true;
figure;
imshow(L);
fprintf('imshow(logical) OK (range [0,1], all pixels white)\n');

fprintf('imshow smoke DONE\n');
