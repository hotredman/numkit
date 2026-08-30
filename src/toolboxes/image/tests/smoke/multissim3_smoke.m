clear

% multissim3 — volumetric multi-scale SSIM (Wang/Simoncelli/Bovik 2003).

[X, Y, Z] = ndgrid(1:16, 1:16, 1:16);
A = uint8(min(255, X + Y + Z));
B = uint8(min(255, X + Y + 2*Z));

fprintf('=== Default ===\n');
fprintf('score = %.10f (expect ~0.9332945347)\n', multissim3(A, B));

fprintf('\n=== NumScales=3 ===\n');
fprintf('score = %.10f (expect ~0.9366688728)\n', multissim3(A, B, 'NumScales', 3));

fprintf('\n=== NumScales=1 ===\n');
fprintf('score = %.10f (expect ~0.9325174093)\n', multissim3(A, B, 'NumScales', 1));

fprintf('\n=== Wang 2003 ScaleWeights ===\n');
W = [0.0448 0.2856 0.3001 0.2363 0.1333];
fprintf('score = %.10f (expect ~0.9366778731)\n', multissim3(A, B, 'ScaleWeights', W));

fprintf('\n=== Sigma=0.5 ===\n');
fprintf('score = %.10f (expect ~0.9694747925)\n', multissim3(A, B, 'Sigma', 0.5));

fprintf('\n=== DynamicRange=128 ===\n');
fprintf('score = %.10f (expect ~0.9089443684)\n', multissim3(A, B, 'DynamicRange', 128));

fprintf('\n=== Identical ===\n');
fprintf('score = %.10f (expect 1)\n', multissim3(A, A));

fprintf('\n=== double ===\n');
Ad = double(A)/255; Bd = double(B)/255;
fprintf('score = %.10f (expect ~0.9332950389)\n', multissim3(Ad, Bd));

fprintf('\n=== quality maps ===\n');
[s, q] = multissim3(A, B);
fprintf('numel(q) = %d\n', length(q));
fprintf('  q{1} size = [%d %d %d]\n', size(q{1},1), size(q{1},2), size(q{1},3));
fprintf('  q{3} size = [%d %d %d]\n', size(q{3},1), size(q{3},2), size(q{3},3));
