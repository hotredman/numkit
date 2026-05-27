clear
import compat.*

% imnlmfilt — Non-Local Means denoising (Buades-Coll-Morel 2005).

I = double(reshape(1:441, 21, 21)) / 441;

fprintf('=== default (auto DegreeOfSmoothing via Immerkaer) ===\n');
[B, estDoS] = imnlmfilt(I);
fprintf('B(11,11) = %.6f (expect 0.501134)\n', B(11,11));
fprintf('estDoS = %.6g (expect 4.747e-3)\n', estDoS);

fprintf('\n=== custom DegreeOfSmoothing ===\n');
B = imnlmfilt(I, 'DegreeOfSmoothing', 0.05);
fprintf('B(11,11) = %.6f (expect 0.501134)\n', B(11,11));

fprintf('\n=== custom ComparisonWindowSize ===\n');
B = imnlmfilt(I, 'ComparisonWindowSize', 3);
fprintf('B(11,11) = %.6f (expect 0.501134)\n', B(11,11));

fprintf('\n=== custom SearchWindowSize ===\n');
B = imnlmfilt(I, 'SearchWindowSize', 11);
fprintf('B(11,11) = %.6f (expect 0.501134)\n', B(11,11));

fprintf('\n=== uint8 input ===\n');
Bu = imnlmfilt(uint8(I*255));
fprintf('B(11,11) = %d (expect 128) class=%s\n', Bu(11,11), class(Bu));
