clear

import compat.*

% iptnum2ordinal — convert positive integer to ordinal-form string.

fprintf('1   = %s (expect first)\n',     iptnum2ordinal(1));
fprintf('2   = %s (expect second)\n',    iptnum2ordinal(2));
fprintf('3   = %s (expect third)\n',     iptnum2ordinal(3));
fprintf('11  = %s (expect eleventh)\n',  iptnum2ordinal(11));
fprintf('12  = %s (expect twelfth)\n',   iptnum2ordinal(12));
fprintf('20  = %s (expect twentieth)\n', iptnum2ordinal(20));

fprintf('\n--- suffix form (>20) ---\n');
fprintf('21  = %s (expect 21st)\n',  iptnum2ordinal(21));
fprintf('22  = %s (expect 22nd)\n',  iptnum2ordinal(22));
fprintf('23  = %s (expect 23rd)\n',  iptnum2ordinal(23));
fprintf('100 = %s (expect 100th)\n', iptnum2ordinal(100));
fprintf('101 = %s (expect 101st)\n', iptnum2ordinal(101));
