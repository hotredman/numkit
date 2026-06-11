clear
import compat.*
% contains / startsWith / endsWith with the 'IgnoreCase' name-value option.
% DEEP-PROBE 2026-05-31: numkit ignored 'IgnoreCase' (always case-sensitive),
% so startsWith('Hello','he','IgnoreCase',true) returned false. Values pinned
% vs MATLAB R2025b.

fprintf('startsWith(Hello,he,IC)   = %d  (expect 1)\n', startsWith('Hello','he','IgnoreCase',true));
fprintf('startsWith(Hello,he)      = %d  (expect 0)\n', startsWith('Hello','he'));
fprintf('endsWith(HELLO,lo,IC)     = %d  (expect 1)\n', endsWith('HELLO','lo','IgnoreCase',true));
fprintf('contains(HeLLo,ell,IC)    = %d  (expect 1)\n', contains('HeLLo','ell','IgnoreCase',true));
fprintf('contains(HeLLo,ell)       = %d  (expect 0)\n', contains('HeLLo','ell'));
% Case-insensitive also works with a cell-of-patterns (any-match).
fprintf('contains(abc,{XYZ,BC},IC) = %d  (expect 1)\n', contains('abc',{'XYZ','BC'},'IgnoreCase',true));
