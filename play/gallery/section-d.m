%========================================================================
% SECTION D — COMMUNICATIONS & MODULATION
%========================================================================

% --- 41. ASK / OOK -----------------------------------------------------
fs = 10e3; symR = 100; ns = fs/symR;
bits = [1 0 1 1 0 1 0 0];
seq = repelem(bits, ns);
t = (0:numel(seq)-1) / fs;
carrier = sin(2*pi*2000*t);
modSig = seq .* carrier;
thr = 0.5 * max(modSig) * ones(1, numel(modSig));
figure; plot(t, modSig, t, thr, 'r--'); title('ASK/OOK waveform');
xlabel('t s'); grid on;

% --- 42. BPSK constellation + BER --------------------------------------
Ns = 200; bitsB = randi([0 1], 1, Ns);
sym = 1 - 2*bitsB;                      % 0 -> +1, 1 -> -1
noise = randn(1, Ns) * 0.3;
rx = sym + noise;
figure; plot(real(rx), imag(rx), '.'); title('BPSK constellation (SNR ~10dB)');
axis([-2 2 -1 1]); grid on;
ebn0 = 0:1:10;
ber = 0.5 * erfc(sqrt(10.^(ebn0/10)));
figure; semilogy(ebn0, ber); title('BPSK BER');
xlabel('Eb/N0 dB'); ylabel('BER'); grid on;

% --- 43. QPSK ------------------------------------------------------------
ph = pi/4 + (0:3)*pi/2;
figure; plot(cos(ph), sin(ph), 'o'); axis([-1.5 1.5 -1.5 1.5]); grid on;
title('QPSK constellation');
rxq = exp(1i*ph(randi(4, 1, Ns))) + (randn(1,Ns)+1i*randn(1,Ns))*0.2;
figure; plot(real(rxq), imag(rxq), '.'); title('QPSK with noise'); grid on;

% --- 44. pi/4-DQPSK: zero-crossing avoidance -----------------------------
dph = pi/4 * (2*randi([0 3],1,Ns-1) + 1);
phPi4 = cumsum(dph);
figure; plot(cos(phPi4), sin(phPi4), '.-'); title('\pi/4-DQPSK trajectory');
axis([-1.5 1.5 -1.5 1.5]); grid on;

% --- 45. 16-QAM with phase noise ------------------------------------------
points = sqrt(2) * ((0:3)-1.5);
[Ix, Iy] = meshgrid(points, points);
Iq = Ix(:) + 1i*Iy(:);
pn = exp(1i * randn(1, numel(Iq)) * 0.15);
rx16 = Iq .* pn + randn(1, numel(Iq)) * 0.05;
figure; plot(real(rx16), imag(rx16), '.'); title('16-QAM, phase noise');
axis equal; grid on;

% --- 46. 64-QAM with AM/AM compression -------------------------------------
step = 2*(0:7)/7 - 1;
[Ix6, Iy6] = meshgrid(step, step);
Iq6 = (Ix6(:) + 1i*Iy6(:));
compressed = sign(real(Iq6)) .* abs(real(Iq6)).^1.3 ...
           + 1i*sign(imag(Iq6)) .* abs(imag(Iq6)).^1.3;
figure; plot(real(Iq6), imag(Iq6), '.', real(compressed), imag(compressed), 'r.');
title('64-QAM ideal vs AM/AM compressed'); legend('ideal','compressed'); grid on;

% --- 47. BFSK orthogonalities ----------------------------------------------
tb = 1e-3; f1 = 2000; f2 = 2500;          % Delta f = 1/(2T) coherent
tg = 0:tb/100:tb;
s1 = sin(2*pi*f1*tg); s2 = sin(2*pi*f2*tg);
corrC = sum(s1.*s2)*100/tb;
f2b = 1500;                               % Delta f = 1/T
s2b = sin(2*pi*f2b*tg);
corrO = sum(s1.*s2b)*100/tb;
figure; plot(tg, s1, tg, s2, tg, s2b); legend('f1','f2 coherent','f2 orthogonal');
title('BFSK: \Deltaf=1/2T vs \Deltaf=1/T'); grid on;

% --- 48. M-FSK spectrogram (hydroacoustic modem) ----------------------------
fs2 = 4000; t3 = 0:1/fs2:0.5;
bitsF = randi([1 4], 1, 20);
sigF = [];
seg = 0:1/fs2:0.024;
for k = 1:numel(bitsF)
    fK = 400 + 200*bitsF(k);
    sigF = [sigF sin(2*pi*fK*seg)]; %#ok<*AGROW>
end
[Sg, fg, tg2] = spectrogram(sigF, hann(128), 96, 256, fs2);
figure; imagesc(tg2, fg, 20*log10(abs(Sg))); colormap('jet');
title('4-FSK spectrogram'); xlabel('t s'); ylabel('f Hz');

% --- 49. MSK continuous phase ------------------------------------------------
tt = 0:1/fs2:0.02;
data = repelem(randi([0 1],1,10), round(numel(tt)/10));
phM = 2*pi*cumsum(data - 0.5)/(2*fs2)*1000;
figure; plot(tt(1:numel(phM)), cos(phM)); title('MSK constant envelope');
grid on;

% --- 50. GMSK Gaussian prefilter ----------------------------------------------
bt = 0.3; span = 2;
tp = -span:1/fs2:span;
gp = exp(-2*log(2)*tp.^2/(bt*1/fs2*1000)^2); %#ok<NASGU>
imp = exp(-(tp/bt).^2 / (2*log(2)));
figure; plot(tp, imp./max(imp)); title('Gaussian prefilter (BT=0.3)');
xlabel('t'); grid on;

% --- 51. OFDM subcarrier orthogonality / ICI ----------------------------------
Nc = 8; to = 0:1/fs2:2e-3;
base = 1e3; dop = 20;
Sub = zeros(Nc, numel(to));
for k = 1:Nc
    fk = base + (k-1)*100;
    Sub(k,:) = sin(2*pi*fk*to);
end
Sici = sin(2*pi*((base + (0:Nc-1)*100 + dop))' * to);
figure; plot(to, Sub(1,:) + Sub(2,:)); title('OFDM: sum of orthogonal subcarriers');
grid on;
figure; plot(to, Sici(1,:) + Sici(2,:)); title('OFDM: with Doppler ICI');
grid on;

% --- 52. LoRa chirp -------------------------------------------------------------
bw = 125e3; ts = 0:1/bw:8e-3;
chirpU = exp(1i*pi*(-bw/2)*ts + 1i*pi*(bw/8e-3)*ts.^2);
figure; spectrogram(real(chirpU), hann(256), 200, 512, bw);
title('LoRa up-chirp spectrogram');

% --- 53. AM envelope detector -----------------------------------------------------
ta = 0:1/fs2:0.01;
msg = cos(2*pi*300*ta);
m = 0.8;
am = (1 + m*msg) .* sin(2*pi*2000*ta);
np2 = 2^nextpow2(numel(am));
env = abs(hilbert([am(:); zeros(np2 - numel(am), 1)]));
env = env(1:numel(am));
figure; plot(ta, am, ta, env, ta, -env, 'r'); title('AM + envelope (m=0.8)');
grid on;
am2 = (1 + 1.5*msg) .* sin(2*pi*2000*ta);
env2 = abs(hilbert([am2(:); zeros(np2 - numel(am2), 1)]));
env2 = env2(1:numel(am2));
figure; plot(ta, am2, ta, env2, 'r'); title('Overmodulation m=1.5 (phase reversals)');
grid on;

% --- 54. FM Bessel spectrum ---------------------------------------------------------
fm2 = 100; df = 240;              % beta = 2.4 (carrier vanishes)
beta = df / fm2;
ttF = 0:1/fs2:0.1;
msgF = sin(2*pi*fm2*ttF);
instF = 2000 + df * msgF;
phF = 2*pi*cumsum(instF)/fs2;
sigF2 = sin(phF);
[SpF, fF] = pwelch(sigF2, hann(512), 256, 1024, fs2);
figure; plot(fF, 20*log10(SpF)); title('FM spectrum, \beta=2.4 (carrier null)');
xlabel('f Hz'); grid on;
