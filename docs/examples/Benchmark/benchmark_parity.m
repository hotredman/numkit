% Benchmark Parity — covers the Phase 1-9 expansion functions
% (var/std/median/mode, nan*, cum*, any/all, repmat/circshift,
%  permute/cat, unique/ismember/histcounts, medfilt1/dct, interp1).
%
% Same script in MATLAB / Octave / numkit-m IDE → wall-time totals
% to compare interpreter + library performance. Functions missing
% from a runtime are reported as "skipped" so the script always
% completes — useful for Octave's `pkg load signal statistics`
% surface gaps.
%
% Sized so a single-pass kernel does meaningful work (a few ms on
% modern hardware) and the whole script finishes in ~30 s native.
clear

fprintf('\n=== numkit-m parity expansion benchmark ===\n\n')

N    = 1000000;   % vector length for elementwise reductions
Mside = 512;      % matrix side for shape-bound ops
Nf    = 16384;    % FFT-backed DSP signal length
Nq    = 50000;    % interp / polyval query count
Re    = 20;       % repetitions per kernel

rng(42);
x = randn(N, 1);
xn = x; xn(rand(N,1) < 0.1) = NaN;
A = randn(Mside, Mside);
xs = randn(Nf, 1);
b32 = fir1(32, 0.25);
xi = round(x * 1000) + 1000;
yi = round(randn(N/4, 1) * 1000) + 1000;
edges = (-3000:300:6000)';
xv = (0:9999)';
yv = sin(xv * 0.01) + 0.1 * randn(10000, 1);
xq = sort(rand(Nq, 1) * 9999);
p15 = randn(1, 16);

% Warm-up — let JITs (V8 / Octave) and Highway dispatch settle.
for w = 1:3
    var(x); std(x); median(x);
    sum(x, 1); cumsum(x); cumprod(x(1:1000));
    any(x > 0); all(x > -100);
    fliplr(A); rot90(A); circshift(A, 5); tril(A);
    permute(A, [2 1]); cat(3, A, A);
    unique(round(x));
end

fprintf('Phase 1 stats (N = %d):\n', N);
b('var(x)',     @() var(x),     Re);
b('std(x)',     @() std(x),     Re);
b('median(x)',  @() median(x),  Re);

fprintf('\nDim overloads (matrix %dx%d):\n', Mside, Mside);
b('sum(M, 1)',  @() sum(A, 1),  Re);
b('sum(M, 2)',  @() sum(A, 2),  Re);
b('mean(M, 2)', @() mean(A, 2), Re);

fprintf('\nPhase 2 NaN-aware (10%% NaN):\n');
b('nansum(xn)',    @() nansum(xn),    Re);
b('nanmean(xn)',   @() nanmean(xn),   Re);
b('nanmedian(xn)', @() nanmedian(xn), Re);

fprintf('\nPhase 3 cumulative + logical:\n');
b('cumsum(x)',          @() cumsum(x),          Re);
b('cumprod(x(1:1000))', @() cumprod(x(1:1000)), Re);
b('cummax(x)',          @() cummax(x),          Re);
b('any(x > 0)',         @() any(x > 0),         Re);
b('all(x > -100)',      @() all(x > -100),      Re);

fprintf('\nPhase 5 manipulation (matrix %dx%d):\n', Mside, Mside);
b('repmat(A, 2, 2)',     @() repmat(A, 2, 2),     Re);
b('fliplr(A)',           @() fliplr(A),           Re);
b('flipud(A)',           @() flipud(A),           Re);
b('rot90(A)',            @() rot90(A),            Re);
b('circshift(A, [3 7])', @() circshift(A, [3 7]), Re);
b('tril(A)',             @() tril(A),             Re);

fprintf('\nPhase 6 N-D manipulation:\n');
b('permute(A, [2 1])', @() permute(A, [2 1]), Re);
b('cat(3, A, A, A)',   @() cat(3, A, A, A),   Re);

fprintf('\nPhase 8 set / search ops:\n');
b('unique(xi)',           @() unique(xi),           Re);
b('ismember(xi, yi)',     @() ismember(xi, yi),     Re);
b('union(xi, yi)',        @() union(xi, yi),        Re);
b('histcounts(x, edges)', @() histcounts(x, edges), Re);

fprintf('\nPhase 9 DSP gaps (signal length %d):\n', Nf);
b('filter(b, 1, x)',   @() filter(b32, 1, xs),   Re);
b('filtfilt(b, 1, x)', @() filtfilt(b32, 1, xs), Re);
b('hilbert(x)',        @() hilbert(xs),          Re);
b('medfilt1(x, 7)',    @() medfilt1(xs, 7),      Re);
b('findpeaks(x)',      @() findpeaks(xs),        Re);
b('dct(x(1:512))',     @() dct(xs(1:512)),       Re);

fprintf('\nInterp / polyval (Nq = %d):\n', Nq);
b('interp1(x,y,xq)',  @() interp1(xv, yv, xq, 'linear'), Re);
b('polyval(p15, xq)', @() polyval(p15, xq),              Re);
b('trapz(yv)',        @() trapz(yv),                     Re);

fprintf('\n=== Benchmark complete ===\n\n')

% Helper b(name, fn, R) lives in b.m beside this script — Octave's
% local-function-in-script support is incomplete, MATLAB needs
% local fns at end (and it's clunky), so a sibling file works
% across all three runtimes.
