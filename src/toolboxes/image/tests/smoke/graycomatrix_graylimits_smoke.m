clear

% graycomatrix now accepts the MATLAB-documented 'GrayLimits', [] (empty) form,
% which means "auto limits = [min(I(:)) max(I(:))] over the actual data" for any
% class. numkit previously threw "GrayLimits must be 2-element" on the empty
% value. The empty form is DISTINCT from the default (class range, e.g. [0 255]
% for uint8): the empty form spreads the NumLevels bins across the data range.

I = uint8([1 2 3 4; 2 3 4 1; 3 4 1 2; 4 1 2 3] * 32);   % values {32,64,96,128}

fprintf('--- empty GrayLimits = data range [32 128] ---\n');
Ge = graycomatrix(I, 'NumLevels', 4, 'GrayLimits', []);
Gx = graycomatrix(I, 'NumLevels', 4, 'GrayLimits', [32 128]);
fprintf('empty == explicit [32 128]? %d   (expect 1)\n', isequal(Ge, Gx));
fprintf('sum(Ge)=%d  Ge(1,2)=%d  Ge(3,4)=%d   (expect 12, 3, 3)\n', ...
        sum(Ge(:)), Ge(1,2), Ge(3,4));

fprintf('--- empty differs from the class-range default [0 255] ---\n');
Gd = graycomatrix(I, 'NumLevels', 4);          % default uint8 -> [0 255]
fprintf('empty differs from default? %d   (expect 1)\n', ~isequal(Ge, Gd));
fprintf('Gd(1,1)=%d  (default crams the data into the low bins -> 0 here)\n', Gd(1,1));

fprintf('--- empty form works for double images too ---\n');
Id = [0.1 0.4 0.7 0.9; 0.4 0.7 0.9 0.1; 0.7 0.9 0.1 0.4; 0.9 0.1 0.4 0.7];
De = graycomatrix(Id, 'NumLevels', 4, 'GrayLimits', []);
Dx = graycomatrix(Id, 'NumLevels', 4, 'GrayLimits', [0.1 0.9]);
fprintf('double empty == explicit [0.1 0.9]? %d   (expect 1)\n', isequal(De, Dx));
