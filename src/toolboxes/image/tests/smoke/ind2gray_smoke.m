clear;

% ── Grayscale-only colormap — identity-ish ────────────────────────
fprintf('--- (1) grayscale colormap ---\n');
cm = [0 0 0; 0.25 0.25 0.25; 0.5 0.5 0.5; 0.75 0.75 0.75; 1 1 1];
disp(ind2gray([1 2 3; 3 4 5], cm));

% ── Off-channel RGB colormap ─────────────────────────────────────
fprintf('\n--- (2) RGB colormap, double index ---\n');
cm_rgb = [1 0 0; 0 1 0; 0 0 1; 0.5 0.5 0; 0.2 0.4 0.6];
g = ind2gray([1 2 3 4 5], cm_rgb);
for k = 1:5
    fprintf('  ind=%d → grey=%.10f\n', k, g(k));
end

% ── uint8 class-preserving lookup ────────────────────────────────
fprintf('\n--- (3) uint8 X — 0-based intlut ---\n');
X8 = uint8([0 1 4; 2 3 4]);
I8 = ind2gray(X8, cm_rgb);
for r = 1:2
    fprintf('  row %d: %3u %3u %3u\n', r, I8(r,1), I8(r,2), I8(r,3));
end

% ── uint16 — past-end clamping at LUT[len] ───────────────────────
fprintf('\n--- (4) uint16 X with past-end value ---\n');
X16 = uint16([0 1 1000; 4 4 4]);
I16 = ind2gray(X16, cm_rgb);
for r = 1:2
    fprintf('  row %d: %5u %5u %5u\n', r, I16(r,1), I16(r,2), I16(r,3));
end

% ── Out-of-range float index → clamp ─────────────────────────────
fprintf('\n--- (5) out-of-range float indices clamp to ends ---\n');
disp(ind2gray([-1 0 6 10], cm_rgb));

% ── Bad-argument validation ──────────────────────────────────────
fprintf('\n--- (6) MAP shape validation ---\n');
try
    ind2gray([1 2 3], [0.5 0.5]);
    fprintf('  NO ERROR (unexpected)\n');
catch e
    fprintf('  ERROR (expected): %s\n', e.message);
end
