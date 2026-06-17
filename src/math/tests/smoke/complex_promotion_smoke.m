clear
import compat.*
% complex-promotion smoke — sqrt/log/log2/log10 of a negative scalar must
% return a complex result (MATLAB parity), not real NaN.
% Run: build/desktop-fast/tests/smoke/Release/numkit_smoke.exe \
%      toolboxes/builtin/tests/smoke/complex_promotion_smoke.m

show = @(n, z) fprintf('%-13s real=%.15g  imag=%.15g  isreal=%d\n', n, real(z), imag(z), isreal(z));

fprintf('--- negative scalar -> complex ---\n');
show('sqrt(-4)',  sqrt(-4));     % expect 0 + 2i
show('log(-1)',   log(-1));      % expect 0 + pi i
show('log(-e)',   log(-exp(1))); % expect 1 + pi i
show('log2(-1)',  log2(-1));     % expect 0 + 4.53236 i
show('log2(-8)',  log2(-8));     % expect 3 + 4.53236 i
show('log10(-1)', log10(-1));    % expect 0 + 1.36438 i
show('log10(-100)', log10(-100));% expect 2 + 1.36438 i

fprintf('\n--- runtime variable (not a literal) also promotes ---\n');
t = -4;
show('t=-4; sqrt(t)', sqrt(t));  % expect 0 + 2i

fprintf('\n--- nonnegative stays real ---\n');
show('sqrt(4)',   sqrt(4));      % expect 2 (real)
show('log2(8)',   log2(8));      % expect 3 (real)
show('log10(100)',log10(100));   % expect 2 (real)
