"""Sections D/E/F/G for the 67-script gallery (appended by gen67.py's
import; provides emit-ready tuples via `collect(emit)`)."""

def collect(emit):
    # ═════════════ D. COMMS ═════════════
    emit('comms', 'mod_ask_ook', 'ASK / OOK',
         'Амплитудная манипуляция, порог компаратора',
"""mDepth = 0.8; fC = 2000; fs = 10e3; symR = 100;
bits = [1 0 1 1 0 1 0 0];
seq = repelem(bits, fs/symR);
t = (0:numel(seq)-1)/fs;
modSig = (1 + mDepth*(seq - 0.5)) .* sin(2*pi*fC*t);
thr = 0.5*max(abs(modSig))*ones(1, numel(modSig));
figure;
plot(t, modSig, t, thr, 'r--');
title('ASK/OOK waveform'); grid on;""",
    [('Сигнал', [{"var": "mDepth", "label": "Индекс m", "min": 0.2, "max": 1.0, "step": 0.1, "default": 0.8},
                {"var": "fC", "label": "Несущая Гц", "min": 500, "max": 4000, "step": 500, "default": 2000}])])

    emit('comms', 'mod_bpsk', 'BPSK',
         'Созвездие + BER(Eb/N0)',
"""noise = 0.3; Ns = 200;
bits = randi([0 1], 1, Ns);
sym = 1 - 2*bits;
rx = sym + noise*randn(1, Ns);
figure;
plot(real(rx), imag(rx), '.');
title('BPSK constellation'); axis([-2 2 -1 1]); grid on;
ebn0 = 0:1:10;
figure;
semilogy(ebn0, 0.5*erfc(sqrt(10.^(ebn0/10))));
title('BPSK BER'); xlabel('Eb/N0 дБ'); grid on;""",
    [('Канал', [{"var": "noise", "label": "Шум σ", "min": 0.05, "max": 1.0, "step": 0.05, "default": 0.3}])])

    emit('comms', 'mod_qpsk', 'QPSK',
         '4 точки, I/Q-квадратуры',
"""noise = 0.2; Ns = 200;
ph = pi/4 + (0:3)*pi/2;
sym = exp(1i*ph(randi(4, 1, Ns)));
rx = sym + (randn(1,Ns) + 1i*randn(1,Ns))*noise;
figure;
plot(real(rx), imag(rx), '.');
title('QPSK with noise'); grid on;""",
    [('Канал', [{"var": "noise", "label": "Шум σ", "min": 0.05, "max": 0.6, "step": 0.05, "default": 0.2}])])

    emit('comms', 'mod_pi4_dqpsk', 'π/4-DQPSK',
         'Без переходов через ноль',
"""Ns = 100;
dph = pi/4*(2*randi([0 3], 1, Ns) + 1);
phPi4 = cumsum(dph);
figure;
plot(cos(phPi4), sin(phPi4), '.-');
title('\\pi/4-DQPSK trajectory'); axis([-2 2 -2 2]); grid on;""",
    [('Символы', [{"var": "Ns", "label": "Символов", "min": 20, "max": 200, "step": 20, "default": 100}])])

    emit('comms', 'mod_16qam_phasenoise', '16-QAM + фазовый шум',
         'Круговое размытие созвездия',
"""phn = 0.15;
points = sqrt(2)*((0:3)-1.5);
[Ix, Iy] = meshgrid(points, points);
Iq = Ix(:) + 1i*Iy(:);
rx = Iq .* exp(1i*randn(1, numel(Iq))*phn) + randn(1, numel(Iq))*0.05;
figure;
plot(real(rx), imag(rx), '.');
title('16-QAM, phase noise'); axis equal; grid on;""",
    [('Гетеродин', [{"var": "phn", "label": "σ фаз. шума", "min": 0.02, "max": 0.4, "step": 0.02, "default": 0.15}])])

    emit('comms', 'mod_64qam_amam', '64-QAM + AM/AM',
         'Компрессия внешних точек',
"""alpha = 1.3;
step6 = 2*(0:7)/7 - 1;
[Ix6, Iy6] = meshgrid(step6, step6);
Iq6 = Ix6(:) + 1i*Iy6(:);
comp = sign(real(Iq6)).*abs(real(Iq6)).^alpha + 1i*sign(imag(Iq6)).*abs(imag(Iq6)).^alpha;
figure;
plot(real(Iq6), imag(Iq6), '.', real(comp), imag(comp), 'r.');
legend('ideal','compressed'); title('64-QAM AM/AM'); grid on;""",
    [('Усилитель', [{"var": "alpha", "label": "Степень", "min": 0.8, "max": 1.5, "step": 0.05, "default": 1.3}])])

    emit('comms', 'mod_bfsk', 'BFSK',
         'Ортогональности Δf=1/2T и Δf=1/T',
"""f2 = 1500;
tb = 1e-3; f1 = 2000;
tg = 0:tb/100:tb;
s1 = sin(2*pi*f1*tg);
s2c = sin(2*pi*f2*tg);
s2o = sin(2*pi*(f1-500)*tg);
figure;
plot(tg, s1, tg, s2c, tg, s2o);
legend('f1','\\Deltaf=1/2T','\\Deltaf=1/T');
title('BFSK orthogonality'); grid on;""",
    [('Частоты', [{"var": "f2", "label": "f₂ Гц", "min": 1200, "max": 1800, "step": 50, "default": 1500}])])

    emit('comms', 'mod_mfsk', '4-FSK спектрограмма',
         'Частотный водопад гидромодема',
"""fBase = 400;
fs2 = 4000;
bitsF = randi([1 4], 1, 20);
seg = 0:1/fs2:0.024;
sigF = [];
for k = 1:numel(bitsF)
    sigF = [sigF sin(2*pi*(fBase + 200*bitsF(k))*seg)];
end
[Sg, fg, tg2] = spectrogram(sigF, hann(128), 96, 256, fs2);
figure;
imagesc(tg2, fg, 20*log10(abs(Sg)));
colormap('jet');
title('4-FSK spectrogram'); xlabel('t s'); ylabel('f Hz');""",
    [('Сигнал', [{"var": "fBase", "label": "База Гц", "min": 200, "max": 800, "step": 100, "default": 400}])])

    emit('comms', 'mod_msk', 'MSK',
         'Непрерывная фаза, постоянная огибающая',
"""bitRate = 10;
fs2 = 4000;
tt = 0:1/fs2:0.02;
data = repelem(randi([0 1], 1, 10), max(1, round(numel(tt)/10)));
data = data(1:numel(tt));
phM = 2*pi*cumsum(data - 0.5)/(2*fs2)*1000;
figure;
plot(tt(1:numel(phM)), cos(phM));
title('MSK constant envelope'); grid on;""",
    [('Сигнал', [{"var": "bitRate", "label": "Бит/с ×10³", "min": 1, "max": 20, "step": 1, "default": 10}])])

    emit('comms', 'mod_gmsk', 'GMSK',
         'Гаусс-префильтр BT=0.2..0.5',
"""bt = 0.3;
fs2 = 4000;
tp = -0.005:1/fs2:0.005;
imp = exp(-(tp/bt).^2/(2*log(2)));
figure;
plot(tp, imp./max(imp));
title('Gaussian prefilter (BT)'); xlabel('t'); grid on;""",
    [('Фильтр', [{"var": "bt", "label": "BT", "min": 0.2, "max": 0.5, "step": 0.05, "default": 0.3}])])

    emit('comms', 'mod_ofdm', 'OFDM: ортогональность и ICI',
         'sinc-поднесущие, доплеровский сдвиг',
"""dop = 20; Nc = 8;
fs2 = 4000;
to = 0:1/fs2:2e-3;
base = 1e3;
Sub = zeros(Nc, numel(to));
for k = 1:Nc
    Sub(k,:) = sin(2*pi*(base + (k-1)*100)*to);
end
Sici = sin(2*pi*(base + (0:Nc-1)*100 + dop)'*to);
figure;
plot(to, Sub(1,:) + Sub(2,:));
title('OFDM: orthogonal sum'); grid on;
figure;
plot(to, Sici(1,:) + Sici(2,:));
title('OFDM: Doppler ICI'); grid on;""",
    [('Канал', [{"var": "dop", "label": "Доплер Гц", "min": 0, "max": 50, "step": 5, "default": 20}])])

    emit('comms', 'mod_css_chirp', 'ЛЧМ-чирп (LoRa)',
         'Спектрограмма пилообразных импульсов',
"""chirpMs = 8;
bw = 125e3;
ts = 0:1/bw:chirpMs*1e-3;
chirpU = exp(1i*pi*(-bw/2)*ts + 1i*pi*(bw/(chirpMs*1e-3))*ts.^2);
figure;
spectrogram(real(chirpU), hann(256), 200, 512, bw);
title('LoRa up-chirp spectrogram');""",
    [('Чирп', [{"var": "chirpMs", "label": "Длительность мс", "min": 2, "max": 16, "step": 2, "default": 8}])])

    emit('comms', 'mod_am_envelope', 'AM: огибающая',
         'm ≤ 1 норма; m > 1 — перемодуляция',
"""m = 0.8;
fs2 = 4000;
ta = 0:1/fs2:0.0625;
msg = cos(2*pi*300*ta);
am = (1 + m*msg).*sin(2*pi*2000*ta);
env = abs(hilbert(am));
figure;
plot(ta, am, ta, env, 'r');
title('AM envelope'); grid on;""",
    [('Модуляция', [{"var": "m", "label": "Индекс m", "min": 0.1, "max": 1.5, "step": 0.1, "default": 0.8}])])

    emit('comms', 'mod_fm_bessel', 'FM: спектр Бесселя',
         'Нуль несущей при β = 2.405',
"""df = 240;
fs2 = 4000;
ttF = 0:1/fs2:0.1;
msgF = sin(2*pi*100*ttF);
instF = 2000 + df*msgF;
phF = 2*pi*cumsum(instF)/fs2;
sigF2 = sin(phF);
[SpF, fF] = pwelch(sigF2, hann(512), 256, 1024, fs2);
figure;
plot(fF, 20*log10(SpF));
title('FM spectrum'); xlabel('f Гц'); grid on;""",
    [('Модуляция', [{"var": "df", "label": "Девиация Гц", "min": 50, "max": 500, "step": 10, "default": 240}])])

    # ═════════════ E. DSP ═════════════
    emit('dsp', 'dsp_biquad_eq', 'Биквад-эквалайзер',
         'f₀, Q, gain — нули/полюса на z-плоскости',
"""f0 = 1000; Q = 4; gainDB = 12; fs3 = 48000;
A = 10^(gainDB/40);
w0 = 2*pi*f0/fs3; al = sin(w0)/(2*Q);
b = [1+al*A, -2*cos(w0), 1-al*A]/(1+al/A);
a = [1, -2*cos(w0)/(1+al/A), (1-al/A)/(1+al/A)];
[H, wf] = freqz(b, a, 512);
figure;
plot(wf/(2*pi), 20*log10(abs(H)));
title('Peaking EQ'); xlabel('norm f'); grid on;
zr = roots(b); pr = roots(a);
figure;
plot(real(zr), imag(zr), 'o', real(pr), imag(pr), 'x');
title('z-plane zeros/poles'); axis([-1.5 1.5 -1.5 1.5]); grid on;""",
    [('EQ', [{"var": "f0", "label": "f₀ Гц", "min": 200, "max": 8000, "step": 200, "default": 1000},
             {"var": "Q", "label": "Q", "min": 0.5, "max": 10, "step": 0.5, "default": 4},
             {"var": "gainDB", "label": "Gain дБ", "min": -12, "max": 18, "step": 3, "default": 12}])])

    emit('dsp', 'dsp_cheby_compare', 'Чебышёв I vs II',
         'Пульсации в ПП vs ПЗ',
"""order = 4; fc = 0.2;
[b1c, a1c] = cheby1(order, 1, fc);
[b2c, a2c] = cheby2(order, 40, fc);
[H1, wf2] = freqz(b1c, a1c, 512);
[H2, ~] = freqz(b2c, a2c, 512);
figure;
plot(wf2/(2*pi), 20*log10(abs(H1)), wf2/(2*pi), 20*log10(abs(H2)));
legend('Cheby I','Cheby II');
title('Chebyshev compare'); grid on;""",
    [('Фильтр', [{"var": "order", "label": "Порядок", "min": 2, "max": 8, "step": 2, "default": 4},
                 {"var": "fc", "label": "Срез ×fs", "min": 0.1, "max": 0.4, "step": 0.05, "default": 0.2}])])

    emit('dsp', 'dsp_fir_kaiser', 'КИХ + Кайзер',
         'β: переходная полоса vs подавление',
"""beta = 5; fc = 0.25;
Nt = 64;
bK = fir1(Nt, fc, kaiser(Nt+1, beta));
[HK, wfK] = freqz(bK, 1, 512);
figure;
plot(wfK/(2*pi), 20*log10(abs(HK)));
title('FIR Kaiser'); xlabel('norm f'); grid on;""",
    [('Синтез', [{"var": "beta", "label": "β", "min": 1, "max": 10, "step": 1, "default": 5},
                 {"var": "fc", "label": "Срез ×fs", "min": 0.1, "max": 0.45, "step": 0.05, "default": 0.25}])])

    emit('dsp', 'dsp_fft_leakage', 'Утечка спектра БПФ',
         'Окна при некратной частоте',
"""fL = 50.5; fs4 = 1000; N4 = 256;
t4 = (0:N4-1)/fs4;
xL = sin(2*pi*fL*t4);
whan = hann(N4)';
wbh = blackmanharris(N4)';
YL = abs(fft(xL));
YL2 = abs(fft(xL .* whan));
YL3 = abs(fft(xL .* wbh));
figure;
plot(20*log10(YL(1:N4/2)), 20*log10(YL2(1:N4/2)), 20*log10(YL3(1:N4/2)));
legend('rect','hann','blackman-harris');
title('Spectral leakage'); grid on;""",
    [('Окно', [{"var": "fL", "label": "f сигнала Гц", "min": 40, "max": 90, "step": 0.5, "default": 50.5}])])

    emit('dsp', 'dsp_hilbert_envelope', 'Гильберт: огибающая',
         'Аналитический сигнал (степень 2 — workaround, см. hilbert bug)',
"""decay = 3;
ts5 = 0:1/1000:0.063;
x5 = cos(2*pi*50*ts5).*exp(-decay*ts5);
z5 = hilbert(x5);
figure;
plot(ts5, x5, ts5, abs(z5));
legend('x(t)','|x(t)|');
title('Analytic signal envelope'); grid on;""",
    [('Сигнал', [{"var": "decay", "label": "Затухание", "min": 1, "max": 8, "step": 1, "default": 3}])])

    # ═════════════ F. CONTROL ═════════════
    emit('control', 'control_pid', 'ПИД-регулятор',
         'Kp/Ki/Kd и переходная характеристика',
"""Kp = 2; Ki = 1; Kd = 0.1;
numC = [Kd Kp Ki];
denOL = [1 1 1 0];
denCL = denOL + [zeros(1, numel(denOL)-numel(numC)) numC];
sys = tf(numC, denCL);
[yy, tt] = step(sys);
figure;
plot(tt, yy);
title('PID step response'); grid on;""",
    [('Регулятор', [{"var": "Kp", "label": "Kp", "min": 0.5, "max": 8, "step": 0.5, "default": 2},
                    {"var": "Ki", "label": "Ki", "min": 0, "max": 4, "step": 0.5, "default": 1},
                    {"var": "Kd", "label": "Kd", "min": 0, "max": 1, "step": 0.05, "default": 0.1}])])

    emit('control', 'control_root_locus', 'Годограф корней',
         'Полюса замкнутой системы vs K',
"""K = 1;
denCL = [1 2 2 + K];
pr = roots(denCL);
figure;
plot(real(pr), imag(pr), 'x');
title('Closed-loop poles at K');
xlabel('Re'); ylabel('Im'); grid on;""",
    [('Усиление', [{"var": "K", "label": "K", "min": 0.1, "max": 5, "step": 0.1, "default": 1}])])

    emit('control', 'control_nyquist', 'Критерий Найквиста',
         'Огибание (-1, j0)',
"""wNy = logspace(-2, 2, 400);
[magNy, phNy] = bode(tf(1, [1 1 1]), wNy);
zNy = magNy(:).*exp(1i*phNy(:)*pi/180);
figure;
plot(real(zNy), imag(zNy));
hold on;
plot(-1, 0, 'rx', 'MarkerSize', 12, 'LineWidth', 2);
hold off;
title('Nyquist vs (-1, j0)'); xlabel('Re'); ylabel('Im'); grid on;""", [])

    emit('control', 'robotics_2link_fk', 'Кинематика 2-звенника',
         'Рабочая зона схвата',
"""L1 = 1.0; L2 = 0.7;
th1 = linspace(0, pi/2, 30);
th2 = linspace(0, pi, 30);
figure;
hold on;
for k = 1:numel(th1)
    xj = L1*cos(th1(k)); yj = L1*sin(th1(k));
    xe = xj + L2*cos(th1(k) + th2(k));
    ye = yj + L2*sin(th1(k) + th2(k));
    plot([0 xj xe], [0 yj ye], 'b-');
    plot(xe, ye, 'r.');
end
plot(0, 0, 'ko', 'MarkerFaceColor', 'k');
title('2-link FK workspace'); axis equal; grid on;""",
    [('Манипулятор', [{"var": "L1", "label": "L₁ м", "min": 0.5, "max": 1.5, "step": 0.1, "default": 1.0},
                     {"var": "L2", "label": "L₂ м", "min": 0.3, "max": 1.2, "step": 0.1, "default": 0.7}])])

    # ═════════════ G. PHYSICS ═════════════
    emit('physics', 'optics_thin_film', 'Просветляющее покрытие',
         'TMM: интерференция в тонкой плёнке',
"""df = 100; nf = 1.38;
n0 = 1.0; ns = 1.5;
lam = linspace(380, 780, 400);
delta = 2*pi*nf*(df*1e-9)./lam;
r01 = (n0-nf)/(n0+nf); r12 = (nf-ns)/(nf+ns);
R = abs((r01 + r12*exp(-2i*delta))./(1 + r01*r12*exp(-2i*delta))).^2;
figure;
plot(lam, R*100);
title('AR coating R(\\lambda)'); xlabel('\\lambda нм'); ylabel('R %'); grid on;""",
    [('Плёнка', [{"var": "df", "label": "Толщина нм", "min": 50, "max": 300, "step": 10, "default": 100},
                 {"var": "nf", "label": "n плёнки", "min": 1.2, "max": 1.8, "step": 0.02, "default": 1.38}])])

    emit('physics', 'optics_airy_disk', 'Диск Эйри',
         'Критерий Рэлея',
"""u = linspace(1e-6, 15, 400);
airy = (2*besselj(1, u)./u).^2;
figure;
plot(u, airy);
title('Airy pattern [2J_1(u)/u]^2');
xlabel('u'); ylabel('intensity'); grid on;""", [])

    emit('physics', 'physics_damped_resonance', 'Резонанс осциллятора',
         'АЧХ, фаза, добротность',
"""zeta = 0.1;
wn = 2*pi*2;
wr = linspace(0.1, 2.3*wn, 400);
Hr = 1./sqrt((1-(wr./wn).^2).^2 + (2*zeta*wr/wn).^2);
figure;
plot(wr/wn, Hr);
title('Resonance, \\zeta'); xlabel('\\omega/\\omega_n'); grid on;""",
    [('Демпфирование', [{"var": "zeta", "label": "ζ", "min": 0.02, "max": 0.8, "step": 0.02, "default": 0.1}])])

    emit('physics', 'physics_lorenz', 'Аттрактор Лоренца',
         'Хаос при росте ρ (ode45)',
"""rho = 28; sigma = 10; beta = 8/3;
fL = @(t, s) [sigma*(s(2)-s(1)); s(1)*(rho-s(3))-s(2); s(1)*s(2)-beta*s(3)];
[tL, yL] = ode45(fL, [0 20], [1 1 1]);
figure;
plot3(yL(:,1), yL(:,2), yL(:,3));
title('Lorenz attractor'); xlabel('x'); ylabel('y'); zlabel('z'); grid on;""",
    [('Система', [{"var": "rho", "label": "ρ", "min": 5, "max": 45, "step": 1, "default": 28}])])
