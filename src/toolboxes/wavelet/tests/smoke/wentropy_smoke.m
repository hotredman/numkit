clear
import compat.*

% wentropy: closed-form additive entropy ("cost") of a coefficient vector.
% bugs/wavelet/wentropy.

x = [0.5 -0.3 0.8 0 -0.1 0.2];
fprintf('shannon    = %.12f  (expect 1.023719175595)\n', wentropy(x, 'shannon'));
fprintf('log energy = %.12f  (expect -12.064573083256)\n', wentropy(x, 'log energy'));
fprintf('threshold  = %g  (expect 3)\n', wentropy(x, 'threshold', 0.2));
fprintf('sure       = %.6f  (expect 0.17)\n', wentropy(x, 'sure', 0.2));
fprintf('norm 1.5   = %.12f  (expect 1.354477406346)\n', wentropy(x, 'norm', 1.5));

fprintf('shannon [1 2 3 4] = %.10f  (expect -69.6816181963)\n', ...
        wentropy([1 2 3 4], 'shannon'));
