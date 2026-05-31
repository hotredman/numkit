clear
import compat.*
% datestr(D, code) numeric format codes 0-31 (MATLAB dateform table).
% DEEP-PROBE 2026-05-31: numeric codes previously threw "not yet supported".
% Added the m (1-letter month), d (1-letter weekday), QQ (quarter) tokens.

dn = datenum(2020,7,28,14,24,5);
fprintf('code 0:  [%s]  (expect 28-Jul-2020 14:24:05)\n', datestr(dn,0));
fprintf('code 2:  [%s]  (expect 07/28/20)\n', datestr(dn,2));
fprintf('code 4:  [%s]  (expect J, single month letter)\n', datestr(dn,4));
fprintf('code 9:  [%s]  (expect T, single weekday letter)\n', datestr(dn,9));
fprintf('code 14: [%s]  (expect " 2:24:05 PM")\n', datestr(dn,14));
fprintf('code 17: [%s]  (expect Q3-20)\n', datestr(dn,17));
fprintf('code 18: [%s]  (expect Q3)\n', datestr(dn,18));
fprintf('code 23: [%s]  (expect 07/28/2020)\n', datestr(dn,23));
fprintf('code 27: [%s]  (expect Q3-2020)\n', datestr(dn,27));
fprintf('code 30: [%s]  (expect 20200728T142405)\n', datestr(dn,30));
fprintf('code 31: [%s]  (expect 2020-07-28 14:24:05)\n', datestr(dn,31));
% New tokens also usable directly in a format string.
fprintf('QQ-yyyy: [%s]  (expect Q3-2020)\n', datestr(dn,'QQ-yyyy'));
fprintf('format string still: [%s]  (expect 2020-07-28)\n', datestr(dn,'yyyy-mm-dd'));
