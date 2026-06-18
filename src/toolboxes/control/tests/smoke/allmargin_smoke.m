clear
import compat.*

% allmargin: all gain/phase/delay margins + closed-loop stability struct.
% bugs/control/allmargin. Exact crossovers via G(jw) scan + bisection.

% 1/((s+1)(s+2)(s+3)): one gain margin (60 at sqrt(11)), no gain crossover.
S = allmargin(tf(1, [1 6 11 6]));
gm = S.GainMargin; gmf = S.GMFrequency; npm = numel(S.PhaseMargin); st = S.Stable;
fprintf('A: GM=%.4f GMf=%.5f (sqrt(11)=%.5f) numPM=%d Stable=%d\n', gm, gmf, sqrt(11), npm, st);
fprintf('   (expect GM=60, GMf=3.31662, numPM=0, Stable=1)\n');

% 1/(s(s+1)(s+2)): GM=6 at sqrt(2), PM=53.41 at 0.4457, DM=2.0913.
S = allmargin(tf(1, [1 3 2 0]));
gm = S.GainMargin; gmf = S.GMFrequency; pm = S.PhaseMargin; pmf = S.PMFrequency; dm = S.DelayMargin; st = S.Stable;
fprintf('B: GM=%.4f GMf=%.5f PM=%.4f PMf=%.5f DM=%.5f Stable=%d\n', gm, gmf, pm, pmf, dm, st);
fprintf('   (expect GM=6, GMf=1.41421, PM=53.4109, PMf=0.44575, DM=2.09131, Stable=1)\n');

% High gain -> closed-loop unstable (exceeds the gain margin of 60).
S = allmargin(tf(100, [1 6 11 6]));
fprintf('C: high-gain Stable=%d  (expect 0)\n', S.Stable);
