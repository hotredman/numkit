clear
import compat.*
% datestr of a CHAR/string date: auto-parse then re-render. DEEP-PROBE
% 2026-05-31: numkit threw "datestr: string date input not yet supported".
% The optional 2nd arg is the OUTPUT format (string or numeric code), NOT
% the input parse spec — the string is auto-detected (ISO / dd-mmm-yyyy).

fprintf('a: [%s]  (expect 30-Dec-2022)\n', datestr('30-Dec-2022'));
fprintf('b: [%s]  (expect 30-Dec-2022)\n', datestr('2022-12-30'));
fprintf('c: [%s]  (expect 12/30/2022)\n', datestr('2022-12-30','mm/dd/yyyy'));
fprintf('d: [%s]  (expect 13:45)\n', datestr('15-Mar-2020 13:45:30','HH:MM'));
fprintf('e: [%s]  (expect 12/30/2022, numeric code 23)\n', datestr('30-Dec-2022',23));
fprintf('f: [%s]  (expect 30-Dec-2022 12:34:56, time auto-included)\n', datestr('2022-12-30 12:34:56'));
