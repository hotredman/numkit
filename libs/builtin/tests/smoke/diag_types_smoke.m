clear
import compat.*
% diag(v|M [,k]) is type-preserving and supports a diagonal offset k.
% DEEP-PROBE 2026-05-31: diag was DOUBLE-only (threw "Not a double array"
% on char/logical/single/int) and silently ignored the k offset.

% --- CHAR vector -> diagonal char matrix (off-diagonal char(0)) ---
C = diag('abc');
fprintf('diag(char vec): class=%s (expect char) size=%dx%d (expect 3x3)\n', class(C), size(C,1), size(C,2));
fprintf('  C(1,1)=%c (expect a) C(2,2)=%c (expect b) double(C(1,2))=%d (expect 0)\n', C(1,1), C(2,2), double(C(1,2)));

% --- CHAR matrix -> extract main diagonal ---
dc = diag(['abc';'def';'ghi']);
fprintf('diag(char mat): class=%s (expect char) size=%dx%d (expect 3x1) [%c%c%c] (expect aei)\n', ...
        class(dc), size(dc,1), size(dc,2), dc(1), dc(2), dc(3));

% --- LOGICAL preserved ---
L = diag(logical([1 0 1]));
fprintf('diag(logical): islogical=%d (expect 1) L(1,1)=%d (expect 1) L(2,2)=%d (expect 0)\n', ...
        islogical(L), L(1,1), L(2,2));

% --- SINGLE / COMPLEX preserved ---
S = diag(single([1 2]));
fprintf('diag(single): class=%s (expect single)\n', class(S));
Z = diag([1+2i 3+4i]);
fprintf('diag(complex): Z(1,1)=%s (expect 1+2i) Z(1,2)=%s (expect 0)\n', mat2str(Z(1,1)), mat2str(Z(1,2)));

% --- k offset: build on superdiagonal / subdiagonal ---
vk = diag([1 2 3], 1);
fprintf('diag([1 2 3],1): size=%dx%d (expect 4x4) vk(1,2)=%g (expect 1) vk(2,3)=%g (expect 2)\n', ...
        size(vk,1), size(vk,2), vk(1,2), vk(2,3));
vn = diag([1 2 3], -1);
fprintf('diag([1 2 3],-1): vn(2,1)=%g (expect 1) vn(3,2)=%g (expect 2)\n', vn(2,1), vn(3,2));

% --- k offset: extract diagonal ---
M = reshape(1:9,3,3);
dk = diag(M, 1);
fprintf('diag(M,1): size=%dx%d (expect 2x1) dk=[%g %g] (expect 4 8)\n', size(dk,1), size(dk,2), dk(1), dk(2));
dn = diag(M, -1);
fprintf('diag(M,-1): dn=[%g %g] (expect 2 6)\n', dn(1), dn(2));
