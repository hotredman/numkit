clear
import compat.*

% imhistmatch(I, ref, nbins) == histeq(I, imhist(ref, nbins)), matching
% MATLAB R2025b:
%   * nbins defaults to 64 for EVERY class (was max(per-class imhist
%     default)=256 for uint8 -> J off by 1-2 levels);
%   * the transform is built at the input class's FULL resolution
%     (NPTS=256 for uint8), NOT at nbins, so each input level maps
%     individually;
%   * the 2nd output hgram = imhist(ref, nbins) is now returned
%     ([J,hgram]=imhistmatch(...) previously threw 'undefined function T').

A = uint8([10 40 70 100; 130 160 190 220; 5 15 25 35; 200 210 230 250]);
R = uint8([0 0 50 50; 100 100 150 150; 200 200 255 255; 60 60 90 90]);

[J, hg] = imhistmatch(A, R);                 % default nbins = 64
fprintf('--- imhistmatch(A,R) default n=64 ---\n');
fprintf('J(1)=%d J(2)=%d J(4)=%d J(16)=%d   (expect 0,101,150,255)\n', ...
        J(1), J(2), J(4), J(16));
fprintf('hgram: size=%dx%d sum=%d hg(1)=%d hg(13)=%d hg(64)=%d   (expect 1x64,16,2,2,2)\n', ...
        size(hg,1), size(hg,2), sum(hg), hg(1), hg(13), hg(64));

[J16, hg16] = imhistmatch(A, R, 16);         % explicit nbins = 16
fprintf('--- imhistmatch(A,R,16) ---\n');
fprintf('J16(1)=%d J16(4)=%d J16(16)=%d   (expect 0,153,255)\n', ...
        J16(1), J16(4), J16(16));
fprintf('hg16(1)=%d hg16(5)=%d sum=%d   (expect 2,2,16)\n', ...
        hg16(1), hg16(5), sum(hg16));

% double image in [0,1]: output levels are k/(n-1) = k/7.
Ad = [0.1 0.4 0.7; 0.2 0.5 0.8; 0.05 0.35 0.95];
Rd = [0 0.5 0.5; 0.5 1 1; 0 0.25 0.75];
[Jd, hgd] = imhistmatch(Ad, Rd, 8);
fprintf('--- imhistmatch(double, double, 8) ---\n');
fprintf('Jd(1)=%.6f Jd(2)=%.6f Jd(8)=%.6f   (expect 0, 0.285714, 1)\n', ...
        Jd(1), Jd(2), Jd(8));
fprintf('hgd sum=%d   (expect 9)\n', sum(hgd));
