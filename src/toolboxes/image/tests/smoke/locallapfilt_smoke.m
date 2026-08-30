clear

% locallapfilt — fast local Laplacian filtering (Aubry/Paris 2014).
% Reference values from MATLAB R2025b probe at tmp/llf_probe.m.

I = uint8([10 20 30 40 50 60 70 80; ...
           20 80 80 80 80 80 80 70; ...
           30 80 200 200 200 200 80 60; ...
           40 80 200 250 250 200 80 50; ...
           50 80 200 250 250 200 80 40; ...
           60 80 200 200 200 200 80 30; ...
           70 80 80 80 80 80 80 20; ...
           80 70 60 50 40 30 20 10]);

fprintf('=== Default: locallapfilt(I, 0.3, 0.4) ===\n');
B = locallapfilt(I, 0.3, 0.4);
fprintf('class=%s size=[%d %d]\n', class(B), size(B,1), size(B,2));
fprintf('B(3,3)=%d (expect ~195)  B(4,4)=%d (expect 255)\n', B(3,3), B(4,4));

fprintf('\n=== beta=0.8 (compression) ===\n');
B = locallapfilt(I, 0.3, 0.4, 0.8);
fprintf('B(3,3)=%d (expect ~187)  B(4,4)=%d (expect ~247)\n', B(3,3), B(4,4));

fprintf('\n=== alpha=2.0 (smoothing) ===\n');
B = locallapfilt(I, 0.3, 2.0);
fprintf('B(3,3)=%d (expect ~203)  B(4,4)=%d (expect ~239)\n', B(3,3), B(4,4));

fprintf('\n=== alpha=1,beta=1 passthrough ===\n');
B = locallapfilt(I, 0.3, 1.0, 1.0);
fprintf('isequal(B,I) = %d\n', isequal(B,I));

fprintf('\n=== sigma=0 passthrough ===\n');
B = locallapfilt(I, 0, 0.4, 1.0);
fprintf('isequal(B,I) = %d\n', isequal(B,I));

fprintf('\n=== flat image ===\n');
F = uint8(100*ones(8,8));
B = locallapfilt(F, 0.3, 0.4);
fprintf('isequal(B,F) = %d\n', isequal(B,F));

fprintf('\n=== NumIntensityLevels=1 (single sample) ===\n');
B = locallapfilt(I, 0.3, 0.4, 1.0, 'NumIntensityLevels', 1);
fprintf('B(1,1)=%d (expect 10)  B(4,4)=%d (expect 250)\n', B(1,1), B(4,4));

fprintf('\n=== single input ===\n');
Is = single(I)/255;
B = locallapfilt(Is, 0.3, 0.4);
fprintf('class=%s  B(4,4)=%.4f\n', class(B), B(4,4));

fprintf('\n=== RGB luminance ===\n');
Irgb = uint8(repmat(reshape(uint8(linspace(0,255,8*8)),8,8),[1 1 3]));
B = locallapfilt(Irgb, 0.3, 0.4);
fprintf('class=%s size=[%d %d %d]\n', class(B), size(B,1), size(B,2), size(B,3));
fprintf('B(4,4,:) = [%d %d %d] (expect ~[105 105 105])\n', B(4,4,1), B(4,4,2), B(4,4,3));

fprintf('\n=== RGB separate ===\n');
B = locallapfilt(Irgb, 0.3, 0.4, 1.0, 'ColorMode', 'separate');
fprintf('B(4,4,:) = [%d %d %d] (expect ~[105 105 105])\n', B(4,4,1), B(4,4,2), B(4,4,3));
