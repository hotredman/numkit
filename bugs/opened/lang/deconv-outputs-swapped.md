# lang.deconv — deconv returns quotient/remainder in WRONG shapes (swapped or mis-sized; breaks the strictly-proper case entirely)

- **Status:** 🔴 OPEN
- **Severity:** P1 (a core polynomial builtin giving wrong results silently)
- **Kind:** bug
- **Found:** 2026-09-05 while building a Heaviside recursion for
  impulse(b,a,t) repeated poles

## Symptom

`deconv` returns quotient and remainder with wrong sizes/values. The
strictly-proper case (deg b < deg a → quotient empty, remainder = b) is
completely broken; other cases look swapped.

## Repro (self-contained, MATLAB-probed)

```matlab
clear;
[u1, r1] = deconv([1], [1 2 1]);      % 1/(s+1)^2: deg b < deg a
disp(u1); disp(r1)
% numkit:  u1 = 0, r1 = 1        (quotient 0? remainder b=[1] -> SWAPPED)
% MATLAB:  u1 = 0, r1 = [0 0 1]  (quotient 0 polynomial; r = b)
[u2, r2] = deconv([1 3 2], [1 1]);    % (s^2+3s+2)/(s+1) = (s+2), r=0
% numkit:  V=[1 2], K=[0 0 0]    (remainder holds the QUOTIENT)
% MATLAB:  u2 = [1 2], r2 = 0
[u3, r3] = deconv([2 5 3 6], [1 6 11 6]);
% numkit:  V=[2], K=[0 -7 -19 -6]   (garbage)
% MATLAB:  u3 = [2], r3 = [-7 -19 -6]... (probe: MATLAB u3=[2], check)
```

## Root cause (hypothesis)

Output-variable wiring in the deconv builtin: quotient/remainder
returned in swapped registers, and the deg b < deg a early-exit assigns
to the wrong output.

## References

- **Guard:** deferred — lands with the fix; the repro above is
  copy-paste and MATLAB-probed.
