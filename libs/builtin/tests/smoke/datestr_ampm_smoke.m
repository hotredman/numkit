clear

import compat.*

% datestr AM/PM meridiem token (2026-05-30): an 'AM'/'PM' token (any case)
% in the format switches HH to a 12-hour, space-padded clock and prints
% AM/PM by time of day. numkit previously ignored the token (kept 24-hour
% HH and a literal 'AM'). vs MATLAB R2025b.

fprintf('00:00 -> [%s]  (expect 12:00:00 AM)\n', datestr(0,    'HH:MM:SS AM'));
fprintf('06:00 -> [%s]  (expect  6:00:00 AM)\n', datestr(0.25, 'HH:MM:SS AM'));
fprintf('12:00 -> [%s]  (expect 12:00:00 PM)\n', datestr(0.5,  'HH:MM:SS AM'));
fprintf('16:48 -> [%s]  (expect  4:48:00 PM)\n', datestr(0.7,  'HH:MM:SS AM'));

fprintf('\n''PM'' token == ''AM'' token (placeholder):\n');
fprintf('16:48 -> [%s]  (expect  4:48 PM)\n', datestr(0.7, 'HH:MM PM'));

fprintf('\nfull date + meridiem:\n');
fprintf('[%s]  (expect 12/30/2022 12:00 PM)\n', datestr(738885.5, 'mm/dd/yyyy HH:MM PM'));

fprintf('\nno meridiem token -> HH stays 24-hour:\n');
fprintf('[%s]  (expect 16:48)\n', datestr(0.7, 'HH:MM'));
