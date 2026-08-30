clear

% imgaborfilt — single-filter Gabor magnitude + phase.

I = double(reshape(1:64, 8, 8)) / 64;

fprintf('=== default (wavelength=4, orientation=0) ===\n');
[mag, ph] = imgaborfilt(I, 4, 0);
fprintf('mag(4,4) = %.6f (expect 0.345825)\n', mag(4,4));
fprintf('mag(1,1) = %.6f (expect 0.958531)\n', mag(1,1));
fprintf('ph(4,4)  = %.6f (expect 0.484451)\n', ph(4,4));

fprintf('\n=== orientation = 90 ===\n');
[mag, ph] = imgaborfilt(I, 4, 90);
fprintf('mag(4,4) = %.6f (expect 0.091440)\n', mag(4,4));

fprintf('\n=== SpatialFrequencyBandwidth = 0.5 ===\n');
[mag, ph] = imgaborfilt(I, 8, 45, 'SpatialFrequencyBandwidth', 0.5);
fprintf('mag(4,4) = %g (expect 6.76e-07)\n', mag(4,4));

fprintf('\n=== SpatialAspectRatio = 0.25 ===\n');
[mag, ph] = imgaborfilt(I, 4, 0, 'SpatialAspectRatio', 0.25);
fprintf('mag(4,4) = %.6f (expect 0.693477)\n', mag(4,4));

fprintf('\n=== SINGLE input class ===\n');
Is = single(I);
[mag, ph] = imgaborfilt(Is, 4, 0);
fprintf('mag(4,4) = %.6f (expect 0.345825) class=%s\n', mag(4,4), class(mag));
