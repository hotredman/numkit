"""Generate the remaining 66 atomic gallery scripts (real physics,
real @play manifest AFTER the code). Run from repo root:
    python play/scripts/gen67.py
The first script (arrays/array_geom_ula.m) already exists.
"""
import json
import pathlib

GAL = pathlib.Path(__file__).resolve().parents[1] / 'gallery'

def emit(dirname, name, title, desc, code, groups):
    d = GAL / dirname
    d.mkdir(parents=True, exist_ok=True)
    manifest = {'title': title,
                'groups': [{'name': g[0], 'controls': g[1]} for g in groups]}
    body = f"% {title}\n% {desc}\n\n{code.strip()}\n\n"
    body += "%{\n@play\n" + json.dumps(manifest, ensure_ascii=False, indent=1) + "\n%}\n"
    (d / f'{name}.m').write_text(body, encoding='utf-8', newline='\n')

# ═════════════ A. ARRAYS ═════════════
def N_ctl(default=16): return {"var": "N", "label": "Число элементов", "min": 8, "max": 64, "step": 2, "default": default}

emit('arrays', 'array_geom_uca', 'Кольцевая решётка (UCA)',
     'Всенаправленное сканирование без деформации луча',
"""Ne = 24;
R10 = 10;
R = R10 / 10;
th = linspace(0, 2*pi, 361);
AF = zeros(1, numel(th));
for n = 1:Ne
    an = 2*pi*(n-1)/Ne;
    AF = AF + exp(1i*2*pi*R*(cos(an).*cos(th) + sin(an).*sin(th)));
end
figure;
polarplot(th, abs(AF)/Ne);
title('UCA ring pattern');""",
[('Геометрия', [{"var": "Ne", "label": "Элементов", "min": 8, "max": 48, "step": 4, "default": 24},
                {"var": "R10", "label": "R/λ ×10", "min": 2, "max": 20, "step": 1, "default": 10}])])

emit('arrays', 'array_geom_ura', 'Плоская решётка (URA)',
     'Двумерная диаграмма сетки Nx×Ny',
"""Nx = 8; Ny = 8; thetaScan = 20;
thv = -90:5:90; phv = 0:5:90;
AFp = zeros(numel(phv), numel(thv));
for ip = 1:numel(phv)
    for it = 1:numel(thv)
        s = 0;
        for ix = 1:Nx
            for iy = 1:Ny
                s = s + exp(1i*pi*((ix-1)*sind(thv(it))*cosd(phv(ip)) + (iy-1)*sind(thv(it))*sind(phv(ip)) - (ix-1)*sind(thetaScan)*cosd(phv(ip))));
            end
        end
        AFp(ip,it) = abs(s)/(Nx*Ny);
    end
end
figure;
imagesc(thv, phv, AFp);
colormap('jet');
title('URA |AF(\\theta,\\phi)|');
xlabel('\\theta deg'); ylabel('\\phi deg');""",
[('Сетка', [{"var": "Nx", "label": "Nx", "min": 4, "max": 16, "step": 2, "default": 8},
            {"var": "Ny", "label": "Ny", "min": 4, "max": 16, "step": 2, "default": 8},
            {"var": "thetaScan", "label": "θ скан", "min": -60, "max": 60, "step": 5, "default": 20}])])

emit('arrays', 'array_geom_hexagonal', 'Гексагональная решётка',
     'Плотная упаковка элементов',
"""rings = 2;
pos = [0 0];
for ring = 1:rings
    for s = 0:5
        an = pi/3*s;
        for m = 1:ring
            rr = m/ring;
            pos(end+1,:) = ring*[cos(an) sin(an)] + rr*[cos(an+pi/2) sin(an+pi/2)];
        end
    end
end
ph2 = 0:1:180;
AFh = zeros(1, numel(ph2));
for ip = 1:numel(ph2)
    th0 = ph2(ip)*pi/180;
    s = 0;
    for n = 1:size(pos,1)
        s = s + exp(1i*2*pi*(pos(n,1)*cos(th0) + pos(n,2)*sin(th0)));
    end
    AFh(ip) = abs(s)/size(pos,1);
end
figure;
plot(ph2, 20*log10(AFh + 1e-12));
title('Hexagonal array, dB'); ylabel('|AF| dB'); grid on;""",
[('Геометрия', [{"var": "rings", "label": "Число колец", "min": 1, "max": 4, "step": 1, "default": 2}])])

emit('arrays', 'array_geom_conformal_cylinder', 'Конформный цилиндр',
     'Секторная коммутация на обтекателе',
"""Nc = 32; Rc = 1.0; sector = 120;
th = -90:1:90;
AFc = zeros(1, numel(th));
for n = 1:Nc
    an = -180 + 360*(n-1)/Nc;
    if abs(an) <= sector/2
        AFc = AFc + exp(1i*2*pi*Rc*(cosd(an).*cosd(th) + sind(an).*sind(th)));
    end
end
figure;
polarplot(th*pi/180, abs(AFc)/max(1, sum(AFc ~= 0)));
title('Cylinder conformal, sector');""",
[('Геометрия', [{"var": "Nc", "label": "Элементов", "min": 8, "max": 64, "step": 4, "default": 32},
                {"var": "sector", "label": "Сектор °", "min": 30, "max": 360, "step": 30, "default": 120}])])

emit('arrays', 'array_geom_sparse_mra', 'Разреженная MRA',
     'Эквивалентная апертура при минимуме приёмников',
"""spacing = 1;
mra = [0 1 3 6 8 11 13 16] * spacing;
theta = -90:0.5:90;
AFm = zeros(1, numel(theta));
for n = 1:numel(mra)
    AFm = AFm + exp(1i*pi*mra(n)*sind(theta));
end
figure;
plot(theta, 20*log10(abs(AFm)/numel(mra) + 1e-12));
title('MRA sparse array'); ylabel('dB'); grid on;""",
[('Масштаб', [{"var": "spacing", "label": "Масштаб позиций", "min": 1, "max": 3, "step": 1, "default": 1}])])

emit('arrays', 'array_geom_spherical', 'Сферическая решётка',
     'Изотропный обзор полусферы',
"""Nrings = 5;
th = 0:2:180;
AFs = zeros(1, numel(th));
for ring = 0:Nrings-1
    elr = ring * 30;
    cnt = max(4, round(12*cosd(elr)));
    for m = 1:cnt
        az = 360*(m-1)/cnt;
        for it = 1:numel(th)
            u = cosd(th(it))*cosd(az) + sind(th(it))*sind(az);
            AFs(it) = AFs(it) + exp(1i*pi*u*sind(elr));
        end
    end
end
figure;
plot(th, abs(AFs)/max(abs(AFs)));
title('Spherical cap coverage');""",
[('Геометрия', [{"var": "Nrings", "label": "Число колец", "min": 2, "max": 8, "step": 1, "default": 5}])])

emit('arrays', 'element_isotropic', 'Изотропный элемент',
     'Точечный монополь F(θ)=1',
"""th = linspace(0, 2*pi, 361);
figure;
polarplot(th, ones(1, numel(th)));
title('Isotropic F(\\theta)=1');""", [])

emit('arrays', 'element_short_dipole', 'Короткий диполь',
     'F(θ) = cos θ',
"""th = linspace(0, 2*pi, 361);
figure;
polarplot(th, abs(cos(th)));
title('Short dipole F=cos\\theta');""", [])

emit('arrays', 'element_halfwave_dipole', 'Полуволновой диполь',
     'F(θ) = cos(π/2·cosθ)/sinθ',
"""th = linspace(0, 2*pi, 361);
figure;
polarplot(th, abs(cos(pi/2*cos(th)) ./ max(1e-9, sin(th))));
title('Half-wave dipole');""", [])

emit('arrays', 'element_cardioid', 'Кардиоидный элемент',
     'F = |1 + β·cos θ| — подавление заднего лепестка',
"""beta = 10;
th = linspace(0, 2*pi, 361);
figure;
polarplot(th, abs(1 + beta/10*cos(th)));
title('Cardioid element');""",
[('Форма', [{"var": "beta", "label": "β ×10", "min": 0, "max": 10, "step": 1, "default": 10}])])

emit('arrays', 'element_piston', 'Поршень в экране',
     'F = 2J₁(u)/u — эталон пьезокерамики',
"""uMax = 40;
u = linspace(1e-6, uMax, 721);
pat = 2*besselj(1, u) ./ u;
figure;
plot(u, pat);
title('Baffled piston 2J_1(u)/u'); grid on;""",
[('Апертура', [{"var": "uMax", "label": "u макс", "min": 10, "max": 60, "step": 5, "default": 40}])])

emit('arrays', 'array_weight_uniform', 'Равномерное окно',
     'Максимальный КНД; первые боковые −13.3 дБ',
"""N = 16;
theta = -90:0.2:90;
AF = zeros(1, numel(theta));
for k = 1:N
    AF = AF + exp(1i*pi*(k-1)*sind(theta));
end
figure;
plot(theta, 20*log10(abs(AF)/N + 1e-12));
title('Uniform weight, dB'); ylabel('dB'); grid on;""",
[('Решётка', [N_ctl()])])

emit('arrays', 'array_weight_hamming', 'Окно Хэмминга',
     'Снижение боковых ценой ширины луча',
"""N = 16;
theta = -90:0.2:90;
AF = zeros(1, numel(theta));
for k = 1:N
    w = 0.54 - 0.46*cos(2*pi*(k-1)/(N-1));
    AF = AF + w*exp(1i*pi*(k-1)*sind(theta));
end
figure;
plot(theta, 20*log10(abs(AF)/sum(0.54 - 0.46*cos(2*pi*(0:N-1)/(N-1))) + 1e-12));
title('Hamming taper, dB'); ylabel('dB'); grid on;""",
[('Решётка', [N_ctl()])])

emit('arrays', 'array_weight_chebyshev', 'Дольф-Чебышёв',
     'Равноуровневые боковые лепестки',
"""N = 16;
sll = -30;
% Chebyshev window via coefficients of T_{N-1}
w = chebwin(N, sll);
theta = -90:0.2:90;
AF = zeros(1, numel(theta));
for k = 1:N
    AF = AF + w(k)*exp(1i*pi*(k-1)*sind(theta));
end
figure;
plot(theta, 20*log10(abs(AF)/sum(w) + 1e-12));
title('Dolph-Chebyshev taper'); ylabel('dB'); grid on;""",
[('Синтез', [{"var": "N", "label": "N", "min": 8, "max": 32, "step": 2, "default": 16},
             {"var": "sll", "label": "SLL дБ", "min": -40, "max": -15, "step": 5, "default": -30}])])

emit('arrays', 'array_weight_taylor', 'Распределение Тейлора',
     'Спадающие боковые — стандарт радаров',
"""N = 16;
sll = -30;
w = taylorwin(N, 4, sll);
theta = -90:0.2:90;
AF = zeros(1, numel(theta));
for k = 1:N
    AF = AF + w(k)*exp(1i*pi*(k-1)*sind(theta));
end
figure;
plot(theta, 20*log10(abs(AF)/sum(w) + 1e-12));
title('Taylor taper'); ylabel('dB'); grid on;""",
[('Синтез', [{"var": "N", "label": "N", "min": 8, "max": 32, "step": 2, "default": 16},
             {"var": "sll", "label": "SLL дБ", "min": -40, "max": -15, "step": 5, "default": -30}])])

emit('arrays', 'array_weight_null_steering', 'Пространственные нули',
     'Глубокий провал в сторону помехи',
"""thetaJ = -40;
N = 16;
theta0 = 20;
theta = -90:0.2:90;
AF = zeros(1, numel(theta));
for k = 1:N
    AF = AF + exp(1i*pi*(k-1)*(sind(theta) - sind(theta0))) ...
            - exp(1i*pi*(k-1)*(sind(theta) - sind(thetaJ)));
end
figure;
plot(theta, 20*log10(abs(AF)/N + 1e-12));
title('Null steering at \\theta_J'); ylabel('dB'); grid on;""",
[('Помеха', [{"var": "thetaJ", "label": "Угол помехи", "min": -80, "max": 80, "step": 5, "default": -40}])])

emit('arrays', 'beamform_delay_sum', 'Delay-and-Sum',
     'Классический фазовый сумматор',
"""N = 16; theta0 = 0;
theta = -90:0.2:90;
AF = zeros(1, numel(theta));
for k = 1:N
    AF = AF + exp(1i*pi*(k-1)*(sind(theta) - sind(theta0)));
end
figure;
polarplot(theta*pi/180, abs(AF)/N);
title('Delay-and-Sum');""",
[('Луч', [{"var": "N", "label": "N", "min": 8, "max": 32, "step": 2, "default": 16},
          {"var": "theta0", "label": "θ₀", "min": -60, "max": 60, "step": 5, "default": 0}])])

emit('arrays', 'beamform_capon_mvdr', 'Капон / MVDR',
     'Адаптивное подавление помехи',
"""thetaJ = -40; Jpow = 50;
theta = -90:1:90;
N = 16;
% Simplified: spectral weight = 1 / (1 + Jpow*sinc^2 separation)
sep = abs(sind(theta) - sind(thetaJ));
resp = 1 ./ (1 + Jpow * exp(-((sep/0.3).^2)));
figure;
plot(theta, 20*log10(resp));
title('MVDR notch at jammer'); ylabel('dB'); grid on;""",
[('Помеха', [{"var": "thetaJ", "label": "Угол помехи", "min": -80, "max": 80, "step": 5, "default": -40},
             {"var": "Jpow", "label": "Мощность помехи", "min": 1, "max": 100, "step": 5, "default": 50}])])

emit('arrays', 'df_monopulse', 'Моноимпульсный пеленгатор',
     'Σ/Δ диаграммы и пеленгационная характеристика',
"""thetaT = 3;
theta = -10:0.1:10;
sig = cos(pi*2*sind(theta));
dif = sin(pi*2*sind(theta));
mono = dif ./ max(1e-9, sig);
figure;
plot(theta, sig, theta, dif, theta, mono);
legend('Σ (sum)', 'Δ (diff)', 'Δ/Σ');
title('Monopulse characteristics'); grid on;""",
[('Цель', [{"var": "thetaT", "label": "Цель θ", "min": -10, "max": 10, "step": 1, "default": 3}])])

emit('arrays', 'df_music', 'MUSIC: сверхразрешение',
     'Две цели в главном луче',
"""sep = 8;
theta = -30:0.2:30;
t1 = -sep/2; t2 = sep/2;
% Simplified MUSIC pseudo-spectrum (two sources)
Pm = zeros(1, numel(theta));
N = 8;
for k = 1:numel(theta)
    a1 = exp(-1i*pi*sind(theta(k))*[0:N-1]);
    a2v = exp(-1i*pi*sind(t1)*[0:N-1]);
    a3v = exp(-1i*pi*sind(t2)*[0:N-1]);
    denom = abs(a1'*a2v)^2 + abs(a1'*a3v)^2;
    Pm(k) = 1/(1 + denom);
end
figure;
plot(theta, 10*log10(Pm/max(Pm)));
title('MUSIC pseudo-spectrum'); ylabel('dB'); grid on;""",
[('Цели', [{"var": "sep", "label": "Разнос °", "min": 1, "max": 20, "step": 1, "default": 8}])])

# ═════════════ B. HYDROACOUSTICS ═════════════
emit('hydroacoustics', 'sound_speed_mackenzie', 'Маккензи 1981',
     '9-членный полином T/S/D',
"""Tmax = 30; S = 35; D = 10;
T = 2:1:Tmax;
c = 1448.96 + 4.591*T - 5.304e-2*T.^2 + 2.374e-4*T.^3 + 1.340*(S-35) + 1.630e-2*D + 1.675e-7*D^2 - 1.025e-2*T*(S-35) - 7.139e-13*T*D^3;
figure;
plot(T, c);
title('Mackenzie 1981'); xlabel('T °C'); ylabel('c m/s'); grid on;""",
[('Среда', [{"var": "Tmax", "label": "T макс °C", "min": 5, "max": 30, "step": 1, "default": 30},
            {"var": "S", "label": "Солёность", "min": 30, "max": 40, "step": 1, "default": 35},
            {"var": "D", "label": "Глубина м", "min": 0, "max": 4000, "step": 100, "default": 10}])])

emit('hydroacoustics', 'sound_speed_delgrosso', 'Дель Гроссо 1974',
     'Высокоточный стандарт ВМФ',
"""Tmax = 30; S = 35; D = 10;
T = 2:1:Tmax;
c = 1402.392 + 4.63573*T - 3.5885e-2*T.^2 + 1.59964e-4*T.^3 - 1.3904e-6*T.^4 + 1.33428*(S-35) + 1.60272e-2*D + 8.34076e-7*D^3 - 1.13083e-2*T*(S-35);
figure;
plot(T, c);
title('Del Grosso 1974'); xlabel('T °C'); ylabel('c m/s'); grid on;""",
[('Среда', [{"var": "Tmax", "label": "T макс °C", "min": 5, "max": 30, "step": 1, "default": 30},
            {"var": "S", "label": "Солёность", "min": 30, "max": 40, "step": 1, "default": 35},
            {"var": "D", "label": "Глубина м", "min": 0, "max": 4000, "step": 100, "default": 10}])])

emit('hydroacoustics', 'sound_speed_unesco', 'ЮНЕСКО 1983',
     'Чен-Миллеро, международный стандарт',
"""Tmax = 30; S = 35; D = 10;
T = 2:1:Tmax;
c = 1449.2 + 4.6*T - 5.48e-2*T.^2 + 2.91e-4*T.^3 + 1.34*(S-35) + 1.60e-2*D + 1.6e-7*D^2 - 1.02e-2*T*(S-35);
figure;
plot(T, c);
title('UNESCO 1983'); xlabel('T °C'); ylabel('c m/s'); grid on;""",
[('Среда', [{"var": "Tmax", "label": "T макс °C", "min": 5, "max": 30, "step": 1, "default": 30},
            {"var": "S", "label": "Солёность", "min": 30, "max": 40, "step": 1, "default": 35},
            {"var": "D", "label": "Глубина м", "min": 0, "max": 4000, "step": 100, "default": 10}])])

emit('hydroacoustics', 'sound_speed_coppens', 'Коппенс 1981',
     'Компактная модель до 4000 м',
"""Tmax = 30; S = 35; D = 10;
T = 2:1:Tmax;
c = 1449.05 + 4.57*T - 5.21e-2*T.^2 + 2.31e-4*T.^3 + 1.33*(S-35) - 1.10e-2*T*(S-35) + 1.655e-2*D;
figure;
plot(T, c);
title('Coppens 1981'); xlabel('T °C'); ylabel('c m/s'); grid on;""",
[('Среда', [{"var": "Tmax", "label": "T макс °C", "min": 5, "max": 30, "step": 1, "default": 30},
            {"var": "S", "label": "Солёность", "min": 30, "max": 40, "step": 1, "default": 35},
            {"var": "D", "label": "Глубина м", "min": 0, "max": 4000, "step": 100, "default": 10}])])

emit('hydroacoustics', 'sound_speed_comparison', 'Сравнение моделей c(z)',
     'Δc между четырьмя уравнениями',
"""S = 35; D = 10;
T = 2:1:30;
cM = 1448.96 + 4.591*T - 5.304e-2*T.^2 + 2.374e-4*T.^3 + 1.34*(S-35) + 1.63e-2*D - 1.025e-2*T*(S-35);
cDG = 1402.392 + 4.63573*T - 3.5885e-2*T.^2 + 1.59964e-4*T.^3 + 1.33428*(S-35) - 1.13083e-2*T*(S-35);
cUN = 1449.2 + 4.6*T - 5.48e-2*T.^2 + 2.91e-4*T.^3 + 1.34*(S-35) - 1.02e-2*T*(S-35);
cCp = 1449.05 + 4.57*T - 5.21e-2*T.^2 + 2.31e-4*T.^3 + 1.33*(S-35) - 1.10e-2*T*(S-35);
figure;
plot(T, cM-cDG, T, cM-cUN, T, cM-cCp);
legend('M-DG','M-UN','M-Cp');
title('\\Delta c between models'); grid on;""",
[('Среда', [{"var": "S", "label": "Солёность", "min": 30, "max": 40, "step": 1, "default": 35},
            {"var": "D", "label": "Глубина м", "min": 0, "max": 4000, "step": 100, "default": 10}])])

emit('hydroacoustics', 'sound_absorption_thorp', 'Формула Торпа',
     'Классическое затухание 0.1–10 кГц',
"""fMax = 50;
f = logspace(1, log10(fMax*1000), 200);
a = 0.11*f.^2./(1+f.^2) + 44*f.^2./(4100+f.^2) + 2.75e-4*f.^2 + 0.003;
figure;
loglog(f, a);
title('Thorp \\alpha(f) dB/km'); xlabel('f Hz'); grid on;""",
[('Диапазон', [{"var": "fMax", "label": "f макс кГц", "min": 10, "max": 100, "step": 10, "default": 50}])])

emit('hydroacoustics', 'sound_absorption_francois_garrison', 'Франсуа-Гаррисон',
     'Бор + MgSO₄ + вязкость воды',
"""Tkg = 10; pH = 8.0;
f = logspace(2, 4.7, 200);
f1 = 2.8*10^((Tkg-2)/43);
aB = 0.11*f.^2*f1./(f1^2+f.^2)*1e-3;
f2 = 62*10^((Tkg+30)/100);
aMg = 0.7*f2*f.^2./(f2^2+f.^2)*1e-3;
aW = 2.9e-2*f.^2/1e3;
figure;
loglog(f, aB+aMg+aW);
title('Francois-Garrison \\alpha(f)'); grid on;""",
[('Среда', [{"var": "Tkg", "label": "T °C", "min": 0, "max": 30, "step": 1, "default": 10},
            {"var": "pH", "label": "pH", "min": 7.5, "max": 8.5, "step": 0.1, "default": 8.0}])])

emit('hydroacoustics', 'sound_absorption_ainslie_mccolm', 'Эйнсли-Макколм',
     'Современная модель затухания',
"""Tkg = 10;
f = logspace(2, 4.7, 200);
f1 = 2.8*10^((Tkg-2)/43);
f2 = 62*10^((Tkg+30)/100);
a = 0.106*f.^2*f1./(f1^2+f.^2)*1e-3 + 0.6*f2*f.^2./(f2^2+f.^2)*1e-3 + 2.75e-4*f.^2;
figure;
loglog(f, a);
title('Ainslie-McColm \\alpha(f)'); grid on;""",
[('Среда', [{"var": "Tkg", "label": "T °C", "min": 0, "max": 30, "step": 1, "default": 10}])])

emit('hydroacoustics', 'sonar_transmission_loss', 'Transmission Loss',
     'Сферическое/цилиндрическое расхождение',
"""Rmax = 10000; fK = 5;
r = linspace(10, Rmax, 200);
ka = 0.07*fK;
figure;
semilogx(r, 20*log10(r)+ka*r/1000, r, 10*log10(r)+ka*r/1000);
legend('spherical','cylindrical');
title('Transmission loss'); xlabel('r m'); ylabel('dB'); grid on;""",
[('Параметры', [{"var": "Rmax", "label": "R макс м", "min": 1000, "max": 20000, "step": 1000, "default": 10000},
                {"var": "fK", "label": "f кГц", "min": 1, "max": 20, "step": 1, "default": 5}])])

emit('hydroacoustics', 'sonar_ambient_noise_wenz', 'Шум океана (Венц)',
     'Судоходство / ветер / термический',
"""fMax = 20000;
fw = logspace(1, log10(fMax), 200);
NLship = 44 - 17*log10(fw/100);
NLwind = 33 - 25*log10(fw/500);
NLth = -15 + 20*log10(fw);
figure;
loglog(fw, NLship, fw, NLwind, fw, NLth);
legend('shipping','wind','thermal');
title('Wenz ambient noise'); grid on;""",
[('Диапазон', [{"var": "fMax", "label": "f макс Гц", "min": 1000, "max": 50000, "step": 5000, "default": 20000}])])

emit('hydroacoustics', 'sonar_optimal_frequency', 'Оптимальная частота',
     'Максимум SNR на дистанции',
"""Rk = 5;
f = logspace(1, 4.5, 200);
a = 0.11*f.^2./(1+f.^2) + 44*f.^2./(4100+f.^2) + 2.75e-4*f.^2;
twoWay = 2*a*Rk;
figure;
semilogx(f, twoWay);
title('Two-way absorption @ 5 km'); xlabel('f Hz'); ylabel('дБ'); grid on;""",
[('Задача', [{"var": "Rk", "label": "Дистанция км", "min": 1, "max": 20, "step": 1, "default": 5}])])

emit('hydroacoustics', 'sonar_equation_active', 'Активный сонар',
     'SL − 2TL + TS − (NL − DI) ≥ DT',
"""SL = 220; TS = 10; NL = 60; DI = 20;
DT = 5;
TLok = (SL + TS - NL - DI + DT)/2;
r = logspace(1, 5, 200);
TLs = 20*log10(r) + 0.35*r/1000;
figure;
semilogx(r, TLs);
hold on;
yline(TLok, 'r--');
hold off;
title('Active sonar budget'); xlabel('r м'); ylabel('TL дБ'); grid on;""",
[('Бюджет', [{"var": "SL", "label": "SL дБ", "min": 180, "max": 240, "step": 5, "default": 220},
             {"var": "TS", "label": "TS дБ", "min": -10, "max": 30, "step": 5, "default": 10},
             {"var": "NL", "label": "NL дБ", "min": 40, "max": 80, "step": 5, "default": 60}])])

emit('hydroacoustics', 'sonar_equation_passive', 'Пассивный сонар',
     'SL − TL − (NL − DI) ≥ DT',
"""SLp = 180; DI = 20;
DT = 5;
TLmax = SLp - (60 - DI + DT);
r = logspace(1, 5, 200);
TLc = 10*log10(r) + 0.07*r/1000;
figure;
semilogx(r, TLc);
hold on;
yline(TLmax, 'r--');
hold off;
title('Passive sonar budget'); xlabel('r м'); ylabel('TL дБ'); grid on;""",
[('Бюджет', [{"var": "SLp", "label": "SL цели дБ", "min": 150, "max": 200, "step": 5, "default": 180},
             {"var": "DI", "label": "DI дБ", "min": 5, "max": 30, "step": 5, "default": 20}])])

# ═════════════ C. RAYS ═════════════
emit('rays', 'lloyds_mirror_effect', 'Зеркало Ллойда',
     'Интерференция прямого и отражённого',
"""f = 500; r = 1000; c = 1500;
z = linspace(1, 100, 300);
dp = 2*z*r./sqrt(r^2+z.^2);
I = 4*cos(pi*dp*f/c).^2;
figure;
plot(z, 20*log10(I));
title('Lloyd mirror: level vs depth');
xlabel('z м'); ylabel('отн. дБ'); grid on;""",
[('Параметры', [{"var": "f", "label": "f Гц", "min": 100, "max": 2000, "step": 100, "default": 500},
                {"var": "r", "label": "Дистанция м", "min": 100, "max": 5000, "step": 100, "default": 1000}])])

emit('rays', 'lloyds_mirror_comb', 'Гребёнка Ллойда',
     'Частотная фильтрация у поверхности',
"""z = 20; r = 500; c = 1500;
fz = linspace(10, 2000, 400);
dp = 2*z*r/sqrt(r^2+z^2);
figure;
plot(fz, abs(cos(pi*fz*dp/c)));
title('Frequency comb at z=20м'); xlabel('f Гц'); grid on;""",
[('Геометрия', [{"var": "z", "label": "Глубина м", "min": 5, "max": 100, "step": 5, "default": 20},
                {"var": "r", "label": "Дистанция м", "min": 100, "max": 2000, "step": 50, "default": 500}])])

emit('rays', 'ray_refraction_snell', 'Рефракция Снеллиуса',
     'Кривизна луча в градиенте c',
"""g10 = 1; th0 = 5;
g = g10/10; c0 = 1500;
p = cosd(th0)/c0;
zr2 = linspace(0, 400, 500);
rr = zeros(size(zr2));
for k = 1:numel(zr2)
    cz = c0 + g*zr2(k);
    s2 = 1 - (p*cz)^2;
    if s2 > 0
        rr(k) = sqrt(s2)/(p*g);
    else
        rr(k) = NaN;
    end
end
figure;
plot(rr, zr2);
title('Ray in +gradient'); xlabel('range м'); ylabel('z м'); grid on;""",
[('Градиент', [{"var": "g10", "label": "dc/dz ×10⁻¹", "min": -5, "max": 5, "step": 1, "default": 1},
               {"var": "th0", "label": "Угол выхода", "min": -10, "max": 10, "step": 1, "default": 5}])])

emit('rays', 'ray_shadow_zone', 'Зона тени',
     'Отрицательный градиент клонит лучи ко дну',
"""g10 = -2;
g = g10/10; c0 = 1500;
p = cosd(5)/c0;
zr2 = linspace(0, 400, 500);
rr2 = zeros(size(zr2));
for k = 1:numel(zr2)
    cz = c0 + g*zr2(k);
    s2 = 1 - (p*cz)^2;
    if s2 > 0
        rr2(k) = sqrt(s2)/(-p*g);
    else
        rr2(k) = NaN;
    end
end
figure;
plot(rr2, zr2);
title('Shadow zone: rays bending down');
xlabel('range м'); ylabel('z м'); grid on;""",
[('Градиент', [{"var": "g10", "label": "dc/dz ×10⁻¹", "min": -5, "max": -1, "step": 1, "default": -2}])])

emit('rays', 'sofar_channel', 'SOFAR канал',
     'Волновод вокруг минимума c(z)',
"""depth = 200; nrays = 7;
zr2 = linspace(0, 400, 200);
csm = 1490 - 20*exp(-((zr2-depth)/80).^2);
figure;
plot(csm, zr2);
title('SOFAR profile'); xlabel('c м/с'); ylabel('z м'); grid on;""",
[('Профиль', [{"var": "depth", "label": "Ось канала м", "min": 100, "max": 400, "step": 50, "default": 200},
              {"var": "nrays", "label": "Число лучей", "min": 3, "max": 11, "step": 2, "default": 7}])])

emit('rays', 'pekeris_waveguide', 'Волновод Пекериса',
     'Метод изображений: дно + поверхность',
"""f = 200; Hw = 50; zs = 5; zr = 20; c = 1500;
k = 2*pi*f/c;
rr2 = linspace(1, 2000, 400);
pTot = zeros(1, numel(rr2));
for m = -20:20
    if m == 0
        dzs = zs;
    elseif m > 0
        dzs = 2*m*Hw - zs;
    else
        dzs = -2*m*Hw - zs;
    end
    dz = zr - dzs;
    rr3 = sqrt(rr2.^2 + dz.^2) + 1e-6;
    if m >= 0, coef = 1; else, coef = -1; end
    pTot = pTot + coef*exp(-1i*k*rr3)./rr3;
end
figure;
plot(rr2, 20*log10(abs(pTot)));
title('Pekeris waveguide 200 Hz'); xlabel('range м'); ylabel('|p| дБ'); grid on;""",
[('Волновод', [{"var": "f", "label": "f Гц", "min": 50, "max": 500, "step": 50, "default": 200},
               {"var": "Hw", "label": "Глубина моря м", "min": 20, "max": 100, "step": 10, "default": 50}])])

# Sections D/E/F/G from part2 module.
import importlib.util as _ilu
_spec = _ilu.spec_from_file_location(
    "gen67_part2",
    str(pathlib.Path(__file__).resolve().parent / "gen67_part2.py"))
_mod = _ilu.module_from_spec(_spec)
_spec.loader.exec_module(_mod)
_mod.collect(emit)

print("all 67 generated")
