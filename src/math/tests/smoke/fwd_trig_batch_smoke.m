clear

import compat.*

% Forward-trig batch — spec closure 2026-05-09. 18 functions.
% sin/sind/sinh + cos/cosd/cosh + tan/tand/tanh +
% sec/secd/sech + csc/cscd/csch + cot/cotd/coth.

fprintf('sin(pi/2)  = %.15f  (expect 1)\n',     sin(pi/2));
fprintf('cos(0)     = %.15f  (expect 1)\n',     cos(0));
fprintf('tan(pi/4)  = %.15f  (expect 1)\n',     tan(pi/4));
fprintf('sec(0)     = %.15f  (expect 1)\n',     sec(0));
fprintf('csc(pi/2)  = %.15f  (expect 1)\n',     csc(pi/2));
fprintf('cot(pi/4)  = %.15f  (expect 1)\n',     cot(pi/4));
fprintf('sind(30)   = %.15f  (expect 0.5)\n',   sind(30));
fprintf('cosd(60)   = %.15f  (expect 0.5)\n',   cosd(60));
fprintf('tand(45)   = %.15f  (expect 1)\n',     tand(45));
fprintf('secd(60)   = %.15f  (expect 2)\n',     secd(60));
fprintf('cscd(30)   = %.15f  (expect 2)\n',     cscd(30));
fprintf('cotd(45)   = %.15f  (expect 1)\n',     cotd(45));
fprintf('sinh(1)    = %.15f  (expect 1.17520)\n', sinh(1));
fprintf('cosh(1)    = %.15f  (expect 1.54308)\n', cosh(1));
fprintf('tanh(1)    = %.15f  (expect 0.76159)\n', tanh(1));
fprintf('sech(1)    = %.15f  (expect 0.64805)\n', sech(1));
fprintf('csch(1)    = %.15f  (expect 0.85092)\n', csch(1));
fprintf('coth(1)    = %.15f  (expect 1.31304)\n', coth(1));
