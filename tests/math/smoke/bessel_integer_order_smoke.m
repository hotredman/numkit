clear

import compat.*

% BUG #36 regression guard — integer-order Bessel J/Y/I/K should
% agree on desktop (std::cyl_bessel_*) and WASM (portable shim).
% Smoke probes the values; same script run via numkit_smoke.exe
% (desktop) and via the WASM REPL must print identical numbers.

fprintf('=== Bessel integer order — BUG #36 cross-platform guard ===\n\n');

% besselj — first 4 orders × 4 arguments
fprintf('besselj(n, x):\n');
for n = 0:3
  for x = [0.5, 1.0, 5.0, 10.0]
    fprintf('  J_%d(%4.1f) = %.10f\n', n, x, besselj(n, x));
  end
end

% bessely — first 4 orders × 4 arguments
fprintf('\nbessely(n, x):\n');
for n = 0:3
  for x = [0.5, 1.0, 5.0, 10.0]
    fprintf('  Y_%d(%4.1f) = %.10f\n', n, x, bessely(n, x));
  end
end

% besseli — first 4 orders × 4 arguments
fprintf('\nbesseli(n, x):\n');
for n = 0:3
  for x = [0.5, 1.0, 5.0, 10.0]
    fprintf('  I_%d(%4.1f) = %.10f\n', n, x, besseli(n, x));
  end
end

% besselk — first 4 orders × 4 arguments
fprintf('\nbesselk(n, x):\n');
for n = 0:3
  for x = [0.5, 1.0, 5.0, 10.0]
    fprintf('  K_%d(%4.1f) = %.10f\n', n, x, besselk(n, x));
  end
end

% Asymptotic-regime spot-check (x > 9/20 — portable K_n / I_n cross
% over to asymptotic). Reference values captured from desktop
% std::cyl_bessel_* — the WASM-portable shim must match to 1e-9
% relative (asymptotic limit; K_0 series cancellation eats 8 digits
% so we cross over at x>9).
fprintf('\nAsymptotic regime (x >= 25):\n');
fprintf('  I_0(25) = %.6e (ref 5.774561e+09)\n', besseli(0, 25));
fprintf('  K_0(25) = %.6e (ref 3.464162e-12)\n', besselk(0, 25));
fprintf('  I_3(30) = %.6e (ref 6.711405e+11)\n', besseli(3, 30));
fprintf('  K_3(30) = %.6e (ref 2.471331e-14)\n', besselk(3, 30));

% J recurrence check: 2n/x · J_n(x) = J_{n-1}(x) + J_{n+1}(x)
fprintf('\nRecurrence sanity:\n');
n = 3; x = 7.5;
lhs = 2*n/x * besselj(n, x);
rhs = besselj(n-1, x) + besselj(n+1, x);
fprintf('  J_3 recurrence err  : %.3e\n', abs(lhs - rhs));

% Wronskian sanity: I_0(x)·K_1(x) + I_1(x)·K_0(x) = 1/x
x = 2.0;
w = besseli(0, x) * besselk(1, x) + besseli(1, x) * besselk(0, x);
fprintf('  I/K Wronskian err   : %.3e (expect ~0 — should equal 1/x = %.4f)\n', ...
        abs(w - 1.0/x), 1.0/x);
