clear
import compat.*

% stretchlim uses a class-dependent histogram bin count (MATLAB R2025b):
%   uint8                       -> 256 bins
%   uint16 / int16 / single / double -> 65536 bins
% A fixed 256 bins coarsely quantized the limits on a double image.

s1 = stretchlim([0.1 0.2 0.9 0.95]);
fprintf('double [0.1 0.2 0.9 0.95]  -> [%.10f %.10f]\n', s1(1), s1(2));
fprintf('   (expect 0.1000076295 0.9499961852 = 6554/65535, 62258/65535)\n');

s2 = stretchlim(linspace(0,1,100));
fprintf('double linspace(0,1,100)   -> [%.10f %.10f]\n', s2(1), s2(2));
fprintf('   (expect 0.0101014725 0.9898985275)\n');

% uint8 unchanged (256-level)
su = stretchlim(uint8([10 50 200 250]));
fprintf('uint8 [10 50 200 250]      -> [%.10f %.10f]\n', su(1), su(2));
fprintf('   (expect 0.0392156863 0.9803921569 = 10/255, 250/255)\n');

% imadjust's auto default calls stretchlim and tracks the corrected limits
a = imadjust([0.1 0.5 0.95]);
fprintf('imadjust default            -> [%.4f %.4f %.4f]\n', a(1), a(2), a(3));
