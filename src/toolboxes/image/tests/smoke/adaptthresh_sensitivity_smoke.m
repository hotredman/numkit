clear

% adaptthresh now matches MATLAB R2025b exactly:
%   * sensitivity scale:  T = clip(localStat * (1.6 - sensitivity), 0, 1)
%     (so s=0.5 -> 1.1x the local mean; s=0 -> 1.6x; s=1 -> 0.6x)
%   * default NeighborhoodSize = 2*floor(size(I)/16)+1, which is 1 (no
%     smoothing) for any dimension below 16 — was wrongly clamped up to 3.
% Previously an approximate bias localStat+(0.5-s)*0.1 was used.

% Constant image isolates the sensitivity scale (local mean = c).
C = 0.5 * ones(8,8);
fprintf('const 0.5:  s=0 -> %.4f (1.6c=0.80)\n', max(adaptthresh(C,0)(:)));
fprintf('            s=0.5 -> %.4f (1.1c=0.55)\n', max(adaptthresh(C,0.5)(:)));
fprintf('            s=1 -> %.4f (0.6c=0.30)\n', max(adaptthresh(C,1)(:)));

% Small (6x6) image -> default neighborhood 1: T = pixel * 1.1, clipped.
B = reshape(mod((0:35)*7,11)/11, 6, 6);
Tb = adaptthresh(B, 0.5);
fprintf('6x6 nbh=1:  Tb(3,3)=%.4f (B=10/11 -> clip 1.0)  Tb(1,1)=%.4f (B=0 -> 0.0)\n', Tb(3,3), Tb(1,1));

% Larger (32x32) image -> neighborhood 5 box mean (matches MATLAB).
Bg = reshape(mod((0:1023)*7,101)/101, 32, 32);
Tg = adaptthresh(Bg, 0.5);
fprintf('32x32 nbh=5: Tg(10,10)=%.6f (expect 0.510574)  Tg(16,16)=%.6f (expect 0.557624)\n', Tg(10,10), Tg(16,16));
