clear
import compat.*
% transpose / .' / ' / ctranspose are type-preserving 2-D transposes.
% DEEP-PROBE 2026-05-31: the transpose() builtin used to coerce every input
% to a DOUBLE matrix of element codes, and the .'/' operators threw
% "Transpose not supported for this type" on anything but DOUBLE/COMPLEX.

% --- CHAR ---
C = ['ab';'cd'];
CT = C.';
fprintf('char .'': class=%s (expect char) size=%dx%d (expect 2x2)\n', class(CT), size(CT,1), size(CT,2));
fprintf('  CT(1,2)=%c (expect c)  CT(2,1)=%c (expect b)\n', CT(1,2), CT(2,1));
CF = transpose(C);
fprintf('transpose(char): class=%s (expect char) CF(1,2)=%c (expect c)\n', class(CF), CF(1,2));

% --- LOGICAL ---
L = logical([1 0 1;0 1 0]);
LT = L.';
fprintf('logical .'': islogical=%d (expect 1) size=%dx%d (expect 3x2) LT(3,1)=%d (expect 1)\n', ...
        islogical(LT), size(LT,1), size(LT,2), LT(3,1));

% --- INT8 ---
I = int8([1 2 3;4 5 6]);
IT = I.';
fprintf('int8 .'': class=%s (expect int8) IT(2,1)=%d (expect 2) IT(3,2)=%d (expect 6)\n', ...
        class(IT), IT(2,1), IT(3,2));

% --- SINGLE ---
S = single([1.5 2.5;3.5 4.5]);
ST = S.';
fprintf('single .'': class=%s (expect single) ST(1,2)=%g (expect 3.5)\n', class(ST), ST(1,2));

% --- COMPLEX: .' no conjugate, '/ctranspose conjugates ---
Z = [1+2i 3+4i;5+6i 7+8i];
ZT = Z.';
ZC = Z';
fprintf('cplx .'' ZT(1,2)=%s (expect 5+6i, no conj)\n', mat2str(ZT(1,2)));
fprintf('cplx '' ZC(1,2)=%s (expect 5-6i, conj) ZC(2,1)=%s (expect 3-4i)\n', ...
        mat2str(ZC(1,2)), mat2str(ZC(2,1)));

% --- CELL: off-diagonal swap ---
K = {1 'x'; [2 3] 4};
KT = K.';
fprintf('cell .'': iscell=%d (expect 1) size=%dx%d (expect 2x2) KT{1,2}=%s (expect [2 3]) KT{2,1}=%s (expect x)\n', ...
        iscell(KT), size(KT,1), size(KT,2), mat2str(KT{1,2}), KT{2,1});
