clear

import compat.*

% --- RC: beta=0.25, span=6, sps=4 → length 25, peak at center ---
h = rcosdesign(0.25, 6, 4);
fprintf('--- rcosdesign(0.25, 6, 4) RC ---\n');
fprintf('  length = %d (expect 25)\n', numel(h));
fprintf('  energy = %.6f (expect 1.0 — unit-energy normalisation)\n', sum(h.^2));
% Peak should be at center index (13).
[~, pk] = max(h);
fprintf('  argmax = %d (expect 13, center)\n', pk);
% RC has zero crossings at integer multiples of T (= sps samples)
% from center; for sps=4, center is idx 13, zeros at 13±4=9,17 and 13±8=5,21.
fprintf('  h(9) = %.6e, h(17) = %.6e (expect ~ 0 — Nyquist zero)\n', ...
    h(9), h(17));
fprintf('  h(5) = %.6e, h(21) = %.6e (expect ~ 0 — 2T zero)\n\n', ...
    h(5), h(21));

% --- RRC: same params ---
hr = rcosdesign(0.25, 6, 4, 'sqrt');
fprintf('--- rcosdesign(0.25, 6, 4, ''sqrt'') RRC ---\n');
fprintf('  length = %d (expect 25)\n', numel(hr));
fprintf('  energy = %.6f (expect 1.0)\n\n', sum(hr.^2));

% --- Cascaded RRC ⊛ RRC = RC (matched-filter property) ---
% conv(hr, hr) at center should equal RC peak (after rescaling).
cas = conv(hr, hr);
% Compare to a unit-energy RC of length conv(hr,hr).
rc_ref = rcosdesign(0.25, 12, 4);   % twice the span gives same length 49
% Both are unit-energy by construction. Up to a constant, cas ≈ rc_ref.
ratio = cas(25) / rc_ref(25);   % center indices
fprintf('--- RRC ⊛ RRC vs RC ---\n');
fprintf('  cas[center] / rc[center] = %.4f (constant scaling — both are unit-energy)\n', ratio);

% --- Limit cases ---
% beta=0 → sinc filter, no roll-off
h0 = rcosdesign(0, 4, 4);
fprintf('\n--- rcosdesign(0, 4, 4) — pure sinc ---\n');
fprintf('  length = %d (expect 17)\n', numel(h0));
fprintf('  h0(9) = %.4f (expect ~ peak)\n', h0(9));
fprintf('  h0(5), h0(13) = %.4e, %.4e (expect ~ 0 at ±T)\n', h0(5), h0(13));

% beta=1 → maximally rounded RC
h1 = rcosdesign(1.0, 4, 4);
fprintf('\n--- rcosdesign(1.0, 4, 4) — full roll-off ---\n');
fprintf('  energy = %.6f (expect 1.0)\n', sum(h1.^2));
fprintf('  h1(9) = %.4f (peak)\n', h1(9));

% --- Pulse-shape application via upfirdn (already in libs/signal) ---
% BPSK symbols, upsample-and-shape with RRC.
data = [1 -1 1 -1 1 1 -1 -1];
hr2 = rcosdesign(0.5, 4, 4, 'sqrt');
y = upfirdn(data, hr2, 4);
fprintf('\n--- BPSK ⊛ RRC via upfirdn(data, h, 4) ---\n');
fprintf('  numel(data) = %d, length(h) = %d, sps = 4\n', ...
    numel(data), numel(hr2));
fprintf('  numel(y) = %d (expect numel(data)*sps + numel(h) - 1 = %d)\n', ...
    numel(y), numel(data)*4 + numel(hr2) - 1);
