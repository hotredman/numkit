clear

% zp2sos vs tf2sos surplus-zero placement (MATLAB R2025b conventions).
% zp2sos: #zeros<#poles -> surplus zeros at the ORIGIN (empty section [0 0 g]).
S = zp2sos([0.5; -0.3], [0.2; 0.1; -0.4; 0.6], 2);
[bb, aa] = sos2tf(S);
fprintf('zp2sos->sos2tf bb: %g %g %g %g %g (expect 0 0 2 -0.4 -0.3)\n', bb(1),bb(2),bb(3),bb(4),bb(5));

% tf2sos: reproduces the ORIGINAL b -> surplus zeros LEFT-aligned (empty [g 0 0]).
s2 = tf2sos([1 0.5], [1 -0.3 0.02 0.001]);
[b2, a2] = sos2tf(s2);
fprintf('tf2sos->sos2tf b2: %g %g %g %g (expect 1 0.5 0 0)\n', b2(1),b2(2),b2(3),b2(4));
