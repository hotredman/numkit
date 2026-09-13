%========================================================================
% SECTION E — DSP & FILTERS
%========================================================================

% --- 55. Biquad peaking EQ: zeros/poles on the z-plane -----------------
f0 = 1000; Q = 4; gainDB = 12; fs3 = 48000;
A = 10^(gainDB/40);
w0 = 2*pi*f0/fs3; al = sin(w0)/(2*Q);
b0 = 1 + al*A; b1 = -2*cos(w0); b2 = 1 - al*A;
a0 = 1 + al/A; a1 = -2*cos(w0); a2 = 1 - al/A;
b = [b0 b1 b2]/a0; a = [1 a1/a0 a2/a0];
[H, wf] = freqz(b, a, 512);
figure; plot(wf/(2*pi), 20*log10(abs(H))); title('Peaking EQ response');
xlabel('normalized f'); ylabel('dB'); grid on;
zr = roots(b); pr = roots(a);
figure; plot(real(zr), imag(zr), 'o', real(pr), imag(pr), 'x');
title('z-plane: zeros (o) / poles (x)'); axis([-1.5 1.5 -1.5 1.5]); grid on;

% --- 56. Cheby 1 vs 2 ----------------------------------------------------
[b1c, a1c] = cheby1(4, 1, 0.2);
[b2c, a2c] = cheby2(4, 40, 0.2);
[H1, wf2] = freqz(b1c, a1c, 512);
[H2, ~]    = freqz(b2c, a2c, 512);
figure; plot(wf2/(2*pi), 20*log10(abs(H1)), wf2/(2*pi), 20*log10(abs(H2)));
legend('Chebyshev I (passband ripple)','Chebyshev II (stopband ripple)');
title('Chebyshev I vs II'); xlabel('normalized f'); grid on;

% --- 57. FIR Kaiser window ------------------------------------------------
Nt = 64; fc = 0.25;
for beta = [2 5 8]
    bK = fir1(Nt, fc, kaiser(Nt+1, beta));
    [HK, wfK] = freqz(bK, 1, 512);
    plot(wfK/(2*pi), 20*log10(abs(HK))); hold on;
end
hold off; title('FIR Kaiser: \beta = 2 / 5 / 8');
xlabel('normalized f'); ylabel('dB'); legend('\beta=2','\beta=5','\beta=8'); grid on;

% --- 58. FFT spectral leakage ----------------------------------------------
fs4 = 1000; N4 = 256;
t4 = (0:N4-1)/fs4;
fL = 50.5;                                % non-bin frequency
xL = sin(2*pi*fL*t4);
wrect = ones(1,N4); whan = hann(N4)'; wham = hamming(N4)'; wbh = blackmanharris(N4)';
figure; hold on;
YL = abs(fft(xL .* wrect)); plot(20*log10(YL(1:N4/2)));
YL2 = abs(fft(xL .* whan)); plot(20*log10(YL2(1:N4/2)));
YL3 = abs(fft(xL .* wbh)); plot(20*log10(YL3(1:N4/2)));
hold off; title('Spectral leakage: rect vs hann vs blackman-harris');
legend('rectangular','hann','blackman-harris'); grid on;

% --- 59. Hilbert analytic signal --------------------------------------------
ts5 = 0:1/1000:0.1;
x5 = cos(2*pi*50*ts5) .* exp(-3*ts5);
np5 = 2^nextpow2(numel(x5));
z5 = hilbert([x5 zeros(1, np5 - numel(x5))]);
z5 = z5(1:numel(x5));
figure; plot(ts5, x5, ts5, abs(z5)); title('Analytic signal: |x| envelope');
xlabel('t s'); legend('x(t)','|x(t)|'); grid on;
figure; plot(ts5, unwrap(angle(z5))); title('Instantaneous phase (unwrapped)');
grid on;
