clear

% fspecial('unsharp',alpha) is the 3x3 unsharp contrast-enhancement filter
% (a sharpening Laplacian). MATLAB R2025b:
%   h = [-a  a-1  -a;  a-1  a+5  a-1;  -a  a-1  -a] / (a+1)
% with alpha in [0,1], default 0.2. It sums to 1. numkit previously threw
% 'unknown filter type 'unsharp'' (the type was unimplemented).

u = fspecial('unsharp');         % default alpha = 0.2
fprintf('--- fspecial(''unsharp'') default a=0.2 ---\n');
fprintf('size=%dx%d sum=%.8f\n', size(u,1), size(u,2), sum(u(:)));
fprintf('centre u(2,2)=%.8f   (expect 4.33333333)\n', u(2,2));
fprintf('edge   u(1,2)=%.8f   (expect -0.66666667)\n', u(1,2));
fprintf('corner u(1,1)=%.8f   (expect -0.16666667)\n', u(1,1));

u5 = fspecial('unsharp', 0.5);
fprintf('--- fspecial(''unsharp'', 0.5) ---\n');
fprintf('centre u5(2,2)=%.8f edge u5(1,2)=%.8f (expect 3.66666667, -0.33333333)\n', ...
        u5(2,2), u5(1,2));

u0 = fspecial('unsharp', 0);
fprintf('--- fspecial(''unsharp'', 0) ---\n');
fprintf('centre u0(2,2)=%.8f corner u0(1,1)=%.8f (expect 5, 0)\n', ...
        u0(2,2), u0(1,1));

% Sharpening demo: filtering a flat patch leaves it unchanged (sum=1).
A = ones(5,5);
B = imfilter(A, u);
fprintf('--- imfilter(ones(5), unsharp) interior ---\n');
fprintf('B(3,3)=%.8f (expect ~1, flat region preserved)\n', B(3,3));
