clear

% bwboundaries now returns the label-matrix L and object-count N as the
% 2nd/3rd outputs, matching MATLAB R2025b (previously only B was returned;
% [B,L,N]=bwboundaries(...) threw 'undefined function L'):
%   B : cell column of P-by-2 [row col] boundary traces.
%   L : H-by-W label matrix (objects 1..N).
%   N : number of objects.
% numkit traces objects only (the 'noholes' behaviour), so L/N are exact
% vs MATLAB for inputs WITHOUT holes.

BW = false(5,5); BW(2:4,2:4) = true;            % one solid object
[B, L, N] = bwboundaries(BW);
fprintf('solid: numel(B)=%d N=%d max(L)=%d L(3,3)=%d L(1,1)=%d   (expect 1,1,1,1,0)\n', ...
        numel(B), N, max(L(:)), L(3,3), L(1,1));

BW2 = false(5,5); BW2(1,1)=true; BW2(3,3)=true; BW2(3,4)=true; BW2(4,4)=true;
[B2, L2, N2] = bwboundaries(BW2);
fprintf('two objects: N=%d numel(B)=%d L(1,1)=%d L(4,4)=%d   (expect 2,2,1,2)\n', ...
        N2, numel(B2), L2(1,1), L2(4,4));

[B4, L4, N4] = bwboundaries(BW2, 4);
fprintf('conn=4: N=%d numel(B)=%d\n', N4, numel(B4));

% A string mode flag ('noholes') is accepted and ignored (objects only).
Bn = bwboundaries(BW, 'noholes');
fprintf('noholes flag: numel(B)=%d\n', numel(Bn));
