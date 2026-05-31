clear
import compat.*

fprintf('=== roifilt2 (filter a region of interest) ===\n');

I = magic(6);
BW = false(6,6); BW(2:4,2:4) = true;     % central 3x3 ROI

% Form 1: filter form (Laplacian), only the ROI changes.
h = [0 1 0; 1 -4 1; 0 1 0];
J = roifilt2(h, I, BW);
fprintf('form 1 (h,I,BW): class=%s\n', class(J));
fprintf('  outside ROI J(1,1)=%g == I(1,1)=%g\n', J(1,1), I(1,1));
fprintf('  inside  ROI J(3,3)=%g (expect 63), J(2,2)=%g (expect -108)\n', J(3,3), J(2,2));
F = imfilter(I, h); Jref = I; Jref(BW) = F(BW);
fprintf('  equals full imfilter then mask? %d\n', isequal(J, Jref));

% Form 2: function-handle form, output class follows the handle.
I8 = uint8(magic(6)*5);
J2 = roifilt2(I8, BW, @(x) x*2);
fprintf('form 2 (I,BW,fun) uint8: class=%s J2(1,1)=%g(=I) J2(3,3)=%g(=2*10)\n', ...
        class(J2), double(J2(1,1)), double(J2(3,3)));
J3 = roifilt2(I8, BW, @(x) double(x)+0.5);
fprintf('form 2 class change: class=%s J3(3,3)=%g(=10.5)\n', class(J3), J3(3,3));

fprintf('\n=== validation ===\n');
try; roifilt2(h, I, false(3,3)); catch e; fprintf('size mismatch: %s\n', strtok(e.message, char(10))); end
