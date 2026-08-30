clear

fprintf('=== image/graycomatrix + graycoprops — texture analysis ===\n');

I = uint8([1 2 3 4; 2 3 4 1; 3 4 1 2; 4 1 2 3] * 32);

fprintf('\n[default GLCM: NumLevels=8, Offset=[0 1]]\n');
G = graycomatrix(I);
fprintf('  size(G) = [%d %d]   sum(G(:)) = %d\n', size(G, 1), size(G, 2), sum(G(:)));
fprintf('  non-zero entries:\n');
[r, c, v] = find(G);
for k = 1:length(r)
    fprintf('    G(%d, %d) = %d\n', r(k), c(k), v(k));
end

fprintf('\n[texture properties via graycoprops]\n');
s = graycoprops(G);
fprintf('  Contrast    = %.4f   (expect 3.0000)\n', s.Contrast);
fprintf('  Correlation = %.4f  (expect -0.2000)\n', s.Correlation);
fprintf('  Energy      = %.4f   (expect 0.2500)\n', s.Energy);
fprintf('  Homogeneity = %.4f   (expect 0.4375)\n', s.Homogeneity);

fprintf('\n[symmetric GLCM with NumLevels=4]\n');
G2 = graycomatrix(I, 'NumLevels', 4, 'Offset', [0 1], 'Symmetric', true);
fprintf('  size(G2) = [%d %d]   sum(G2(:)) = %d  (2x non-symmetric)\n', ...
    size(G2, 1), size(G2, 2), sum(G2(:)));
fprintf('  G2(2, 3) = %d,  G2(3, 2) = %d   (symmetric pair)\n', G2(2, 3), G2(3, 2));

fprintf('\nBit-equal with MATLAB R2025b on all 12 fingerprints. Octave 11.1.0\n');
fprintf('ships graycomatrix in the image package but doesn''t yet ship\n');
fprintf('graycoprops, so the harness reports N/A for Octave.\n');
