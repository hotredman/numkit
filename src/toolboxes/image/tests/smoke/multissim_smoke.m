clear

% multissim — multi-scale SSIM (Wang/Simoncelli/Bovik 2003).
% Reference values from MATLAB R2025b on deterministic gradient.

[X, Y] = meshgrid(1:32, 1:32);
A = uint8(min(255, X + Y));
B = uint8(min(255, X + 2*Y));

fprintf('=== Default (NumScales=5, default ScaleWeights) ===\n');
score = multissim(A, B);
fprintf('score = %.10f (expect ~0.8983750939)\n', score);

fprintf('\n=== NumScales=1 (single-scale SSIM equivalent) ===\n');
score = multissim(A, B, 'NumScales', 1);
fprintf('score = %.10f (expect ~0.8935790062)\n', score);

fprintf('\n=== NumScales=3 ===\n');
score = multissim(A, B, 'NumScales', 3);
fprintf('score = %.10f (expect ~0.9119560719)\n', score);

fprintf('\n=== Wang/Simoncelli/Bovik 2003 ScaleWeights ===\n');
W = [0.0448 0.2856 0.3001 0.2363 0.1333];
score = multissim(A, B, 'ScaleWeights', W);
fprintf('score = %.10f (expect ~0.8920533657)\n', score);

fprintf('\n=== Sigma=0.5 (narrower Gaussian) ===\n');
score = multissim(A, B, 'Sigma', 0.5);
fprintf('score = %.10f (expect ~0.9531264305)\n', score);

fprintf('\n=== DynamicRange=128 ===\n');
score = multissim(A, B, 'DynamicRange', 128);
fprintf('score = %.10f (expect ~0.8723636866)\n', score);

fprintf('\n=== Identical images ===\n');
fprintf('multissim(A, A) = %.10f (expect 1)\n', multissim(A, A));

fprintf('\n=== Quality maps ===\n');
[score, qmap] = multissim(A, B);
fprintf('numel(qmap) = %d  (expect 5)\n', length(qmap));
for k = 1:length(qmap)
    sz = size(qmap{k});
    fprintf('  qmap{%d} size = [%d %d]\n', k, sz(1), sz(2));
end

fprintf('\n=== double input ===\n');
Ad = double(A)/255; Bd = double(B)/255;
s = multissim(Ad, Bd);
fprintf('class=%s score=%.10f (expect ~0.8983752005)\n', class(s), s);
