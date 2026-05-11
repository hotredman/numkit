clear
import compat.*

% save/load .mat round-trip — exercises every supported encoding path.
% Writes to a tempdir-derived path, reads back, prints diffs.

fname = fullfile(tempdir(), 'numkit_savemat_smoke.mat');

A = reshape(1:12, 3, 4);
Z = [1+2i 3-1i; 0+0i 4+5i];
L = [true false; false true];
i16 = int16([-100 0 100]);
u8  = uint8([0 128 255]);
s.alpha = [10 20 30];
s.beta  = 'hello';
c = {1, 'two', [3 4 5]};

save(fname, 'A', 'Z', 'L', 'i16', 'u8', 's', 'c');
fprintf('saved → %s\n', fname);

% Wipe everything except fname, then reload.
clear A Z L i16 u8 s c
load(fname);

fprintf('A(1,1)  = %g   (expect 1)\n',  A(1,1));
fprintf('A(3,4)  = %g   (expect 12)\n', A(3,4));
fprintf('Z(1,2)  = %g + %gi  (expect 3 + -1i)\n', real(Z(1,2)), imag(Z(1,2)));
fprintf('L(1,1)  = %g   (expect 1, islogical=%d)\n', double(L(1,1)), islogical(L));
fprintf('i16(3)  = %d   (expect 100, class=%s)\n',  double(i16(3)),  class(i16));
fprintf('u8(2)   = %d   (expect 128, class=%s)\n',  double(u8(2)),   class(u8));
fprintf('s.alpha(3) = %g   (expect 30)\n', s.alpha(3));
fprintf('s.beta     = %s   (expect hello)\n', s.beta);
fprintf('c{1}=%g  c{2}=%s  c{3}(2)=%g (expect 1 / two / 4)\n', ...
        c{1}, c{2}, c{3}(2));

% Also exercise the struct-output form: S = load(...).
clear A Z L i16 u8 s c
S = load(fname);
fprintf('S.A(2,2)  = %g   (expect 5)\n', S.A(2,2));
fprintf('S.s.beta  = %s   (expect hello)\n', S.s.beta);

% Cleanup.
delete(fname);
fprintf('done.\n');
