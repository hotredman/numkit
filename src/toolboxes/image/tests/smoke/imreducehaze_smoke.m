clear

% imreducehaze — dark-channel-prior dehazing (He/Sun/Tang 2011).
% Reference values captured from MATLAB R2025b in tmp/reducehaze_probe.m.

rng(0);
A = uint8(255 * rand(32, 32, 3));
A = uint8(min(255, double(A) + 80));

fprintf('=== Default (simpledcp + global stretch) ===\n');
[B, T, L] = imreducehaze(A);
fprintf('class=%s size=[%d %d %d] L=[%g %g %g] (expect L=[1 1 1])\n', class(B), size(B,1), size(B,2), size(B,3), L(1), L(2), L(3));
fprintf('B(8,8,:) = [%d %d %d] (expect [54 146 255])\n', B(8,8,1), B(8,8,2), B(8,8,3));
fprintf('B(16,16,:) = [%d %d %d] (expect [141 62 255])\n', B(16,16,1), B(16,16,2), B(16,16,3));
fprintf('T(8,8)=%.6f (expect 0.572982) T(16,16)=%.6f (expect 0.560877)\n', T(8,8), T(16,16));

fprintf('\n=== amount=0 passthrough ===\n');
[B, T, L] = imreducehaze(A, 0);
fprintf('isequal(B,A)=%d isempty(T)=%d isempty(L)=%d\n', isequal(B,A), isempty(T), isempty(L));

fprintf('\n=== ContrastEnhancement=none ===\n');
B = imreducehaze(A, 1, 'ContrastEnhancement', 'none');
fprintf('B(8,8,:) = [%d %d %d] (expect [32 121 255])\n', B(8,8,1), B(8,8,2), B(8,8,3));

fprintf('\n=== ContrastEnhancement=boost ===\n');
B = imreducehaze(A, 1, 'ContrastEnhancement', 'boost');
fprintf('B(8,8,:) = [%d %d %d] (expect [34 128 255])\n', B(8,8,1), B(8,8,2), B(8,8,3));

fprintf('\n=== Method=approxdcp ===\n');
[B, T, L] = imreducehaze(A, 1, 'Method', 'approxdcp');
fprintf('L=[%g %g %g] (MATLAB [1 1 0.812])\n', L(1), L(2), L(3));
fprintf('B(8,8,:) = [%d %d %d] (MATLAB [84 159 255]; numkit may differ ~15-30 from histogram-bucket boundaries)\n', B(8,8,1), B(8,8,2), B(8,8,3));

fprintf('\n=== Explicit AtmosphericLight=[0.9 0.9 0.9] ===\n');
B = imreducehaze(A, 1, 'AtmosphericLight', [0.9 0.9 0.9]);
fprintf('B(8,8,:) = [%d %d %d] (expect [76 165 255])\n', B(8,8,1), B(8,8,2), B(8,8,3));

fprintf('\n=== Grayscale input ===\n');
G = rgb2gray(A);
[B, T, L] = imreducehaze(G);
fprintf('class=%s size=[%d %d] L=%g\n', class(B), size(B,1), size(B,2), L);
