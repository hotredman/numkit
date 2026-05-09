
## Closed
- Closed in commit: pending (freqz endpoint fix)
- Closed date: 2026-05-09
- Notes: Initial closure (cycle 43) was DEFERRED. Root cause: freqz used inclusive [0,pi] grid (denominator npts-1); MATLAB uses exclusive [0,pi) (denominator npts). Fix landed in libs/signal/src/filter_analysis/frequency_response.cpp + DC NaN handling for phasez (when |H|=0) and phasedelay (when w=0). Verified bit-identical with MATLAB R2025b on probed inputs. Two existing gtests (FreqzFrequencyRange, PhasezReturnsCorrectShape) updated to expect MATLAB convention W(end) = (n-1)*pi/n instead of pi.
