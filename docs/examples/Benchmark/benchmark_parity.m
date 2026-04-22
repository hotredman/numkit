% Benchmark Parity — covers the Phase 1-9 expansion functions
% (var/std/median/mode, nan*, cum*, any/all, repmat/circshift,
%  permute/cat, unique/ismember/histcounts, medfilt1/dct, interp1).
%
% Same script in MATLAB / Octave / numkit-m IDE → wall-time totals
% to compare interpreter + library performance.
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
b = fir1(32, 0.25);
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
    nansum(xn); nanmean(xn); nanmedian(xn);
    fliplr(A); rot90(A); circshift(A, 5); tril(A);
    permute(A, [2 1]); cat(3, A, A);
    unique(round(x));
end

fprintf('Phase 1 stats (N = %d):\n', N);
tic; for r = 1:Re, var(x);    end; fprintf('  var(x)                   %8.2f ms\n', toc*1000);
tic; for r = 1:Re, std(x);    end; fprintf('  std(x)                   %8.2f ms\n', toc*1000);
tic; for r = 1:Re, median(x); end; fprintf('  median(x)                %8.2f ms\n', toc*1000);

fprintf('\nDim overloads (matrix %dx%d):\n', Mside, Mside);
tic; for r = 1:Re, sum(A, 1);  end; fprintf('  sum(M, 1)                %8.2f ms\n', toc*1000);
tic; for r = 1:Re, sum(A, 2);  end; fprintf('  sum(M, 2)                %8.2f ms\n', toc*1000);
tic; for r = 1:Re, mean(A, 2); end; fprintf('  mean(M, 2)               %8.2f ms\n', toc*1000);

fprintf('\nPhase 2 NaN-aware (10%% NaN):\n');
tic; for r = 1:Re, nansum(xn);    end; fprintf('  nansum(xn)               %8.2f ms\n', toc*1000);
tic; for r = 1:Re, nanmean(xn);   end; fprintf('  nanmean(xn)              %8.2f ms\n', toc*1000);
tic; for r = 1:Re, nanmedian(xn); end; fprintf('  nanmedian(xn)            %8.2f ms\n', toc*1000);

fprintf('\nPhase 3 cumulative + logical:\n');
tic; for r = 1:Re, cumsum(x);          end; fprintf('  cumsum(x)                %8.2f ms\n', toc*1000);
tic; for r = 1:Re, cumprod(x(1:1000)); end; fprintf('  cumprod(x(1:1000))       %8.2f ms\n', toc*1000);
tic; for r = 1:Re, cummax(x);          end; fprintf('  cummax(x)                %8.2f ms\n', toc*1000);
tic; for r = 1:Re, any(x > 0);         end; fprintf('  any(x > 0)               %8.2f ms\n', toc*1000);
tic; for r = 1:Re, all(x > -100);      end; fprintf('  all(x > -100)            %8.2f ms\n', toc*1000);

fprintf('\nPhase 5 manipulation (matrix %dx%d):\n', Mside, Mside);
tic; for r = 1:Re, repmat(A, 2, 2);     end; fprintf('  repmat(A, 2, 2)          %8.2f ms\n', toc*1000);
tic; for r = 1:Re, fliplr(A);           end; fprintf('  fliplr(A)                %8.2f ms\n', toc*1000);
tic; for r = 1:Re, flipud(A);           end; fprintf('  flipud(A)                %8.2f ms\n', toc*1000);
tic; for r = 1:Re, rot90(A);            end; fprintf('  rot90(A)                 %8.2f ms\n', toc*1000);
tic; for r = 1:Re, circshift(A, [3 7]); end; fprintf('  circshift(A, [3 7])      %8.2f ms\n', toc*1000);
tic; for r = 1:Re, tril(A);             end; fprintf('  tril(A)                  %8.2f ms\n', toc*1000);

fprintf('\nPhase 6 N-D manipulation:\n');
tic; for r = 1:Re, permute(A, [2 1]); end; fprintf('  permute(A, [2 1])        %8.2f ms\n', toc*1000);
tic; for r = 1:Re, cat(3, A, A, A);   end; fprintf('  cat(3, A, A, A)          %8.2f ms\n', toc*1000);

fprintf('\nPhase 8 set / search ops:\n');
tic; for r = 1:Re, unique(xi);            end; fprintf('  unique(xi)               %8.2f ms\n', toc*1000);
tic; for r = 1:Re, ismember(xi, yi);      end; fprintf('  ismember(xi, yi)         %8.2f ms\n', toc*1000);
tic; for r = 1:Re, union(xi, yi);         end; fprintf('  union(xi, yi)            %8.2f ms\n', toc*1000);
tic; for r = 1:Re, histcounts(x, edges);  end; fprintf('  histcounts(x, edges)     %8.2f ms\n', toc*1000);

fprintf('\nPhase 9 DSP gaps (signal length %d):\n', Nf);
tic; for r = 1:Re, filter(b, 1, xs);   end; fprintf('  filter(b, 1, x)          %8.2f ms\n', toc*1000);
tic; for r = 1:Re, filtfilt(b, 1, xs); end; fprintf('  filtfilt(b, 1, x)        %8.2f ms\n', toc*1000);
tic; for r = 1:Re, hilbert(xs);        end; fprintf('  hilbert(x)               %8.2f ms\n', toc*1000);
tic; for r = 1:Re, medfilt1(xs, 7);    end; fprintf('  medfilt1(x, 7)           %8.2f ms\n', toc*1000);
tic; for r = 1:Re, findpeaks(xs);      end; fprintf('  findpeaks(x)             %8.2f ms\n', toc*1000);
tic; for r = 1:Re, dct(xs(1:512));     end; fprintf('  dct(x(1:512))            %8.2f ms\n', toc*1000);

fprintf('\nInterp / polyval (Nq = %d):\n', Nq);
tic; for r = 1:Re, interp1(xv, yv, xq, 'linear'); end; fprintf('  interp1(x,y,xq)          %8.2f ms\n', toc*1000);
tic; for r = 1:Re, polyval(p15, xq);              end; fprintf('  polyval(p15, xq)         %8.2f ms\n', toc*1000);
tic; for r = 1:Re, trapz(yv);                     end; fprintf('  trapz(yv)                %8.2f ms\n', toc*1000);

fprintf('\n=== Benchmark complete ===\n\n')
