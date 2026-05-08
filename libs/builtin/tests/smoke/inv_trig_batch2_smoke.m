clear

import compat.*

% Inverse-trig batch 2 — audit ТЗ closure 2026-05-09.
% asin/asind/asinh + atan/atand/atanh + asec/asecd/asech.
% All bit-identical MATLAB R2025b.

fprintf('asin(0.5)  = %.15f  (expect pi/6)\n',  asin(0.5));
fprintf('asind(0.5) = %.15f  (expect 30)\n',    asind(0.5));
fprintf('asinh(1)   = %.15f  (expect 0.881374)\n', asinh(1));
fprintf('atan(1)    = %.15f  (expect pi/4)\n',  atan(1));
fprintf('atand(1)   = %.15f  (expect 45)\n',    atand(1));
fprintf('atanh(0.5) = %.15f  (expect 0.549306)\n', atanh(0.5));
fprintf('asec(2)    = %.15f  (expect pi/3)\n',  asec(2));
fprintf('asecd(2)   = %.15f  (expect 60)\n',    asecd(2));
fprintf('asech(0.5) = %.15f  (expect 1.31696)\n', asech(0.5));
