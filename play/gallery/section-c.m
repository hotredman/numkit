%========================================================================
% SECTION C — RAY THEORY & WAVE PROPAGATION
%========================================================================

% --- 35. Lloyd's mirror: interference pattern --------------------------
f = 500; c = 1500; lambda = c / f;
z = linspace(1, 100, 300);          % receiver depth (m)
r = 1000;                            % range (m)
dpath = 2 * z * r / sqrt(r^2 + z.^2); % path difference approx
phase = 2 * pi * dpath / lambda;
I = 4 * cos(phase / 2).^2;           % interference intensity factor
figure; plot(z, 20*log10(I)); title('Lloyd mirror: level vs depth (500 Hz)');
xlabel('z m'); ylabel('rel dB'); grid on;

% --- 36. Lloyd's mirror frequency comb ----------------------------------
fz = linspace(10, 2000, 400);
zfix = 20; rfix = 500;
dp = 2 * zfix * rfix / sqrt(rfix^2 + zfix^2);
comb = abs(cos(pi * fz * dp / c));
figure; plot(fz, comb); title('Frequency comb at z=20m');
xlabel('f Hz'); grid on;

% --- 37. Snell refraction in linear gradient ---------------------------
c0 = 1500; g = 0.1;                  % m/s per m
zr = linspace(0, 400, 200);
cr = c0 + g * zr;
th0 = 5;                             % launch angle deg
p = cosd(th0) / c0;                  % Snell parameter
zr2 = linspace(0, 400, 500);
rr = zeros(size(zr2));
for k = 1:numel(zr2)
    cz = c0 + g * zr2(k);
    s2 = 1 - (p * cz)^2;
    if s2 > 0
        rr(k) = s2^0.5 / (p * g);
    else
        rr(k) = NaN;                 % ray turned
    end
end
figure; plot(rr, zr2); title('Rounded ray in +g gradient');
xlabel('range m'); ylabel('depth m'); grid on;

% --- 38. Shadow zone (negative gradient) --------------------------------
gn = -0.2;
rr2 = zeros(size(zr2));
for k = 1:numel(zr2)
    cz = c0 + gn * zr2(k);
    s2 = 1 - (p * cz)^2;
    if s2 > 0
        rr2(k) = s2^0.5 / (-p * gn);
    else
        rr2(k) = NaN;
    end
end
figure; plot(rr2, zr2); title('Shadow zone: rays bending down (-g)');
xlabel('range m'); ylabel('depth m'); grid on;

% --- 39. SOFAR channel: rays oscillating around c minimum ---------------
csm = 1490 - 20*exp(-((zr2 - 200)/80).^2);   % channel profile
figure; plot(csm, zr2); title('SOFAR profile c(z)'); xlabel('c m/s'); ylabel('z m'); grid on;
% ray trace via small-angle stepping
nr = 7; ranges = zeros(nr, numel(zr2));
for m = 1:nr
    th = -6 + 2*m;
    x = 0; zz = 100; ang = th;
    for k = 1:numel(zr2)
        ranges(m,k) = x;
        idx = max(1, min(numel(zr2), round(zz) + 1));
        cc = csm(idx);
        dth = -(9.8e-3) * tand(ang) * 10;   % crude gradient bend
        ang = ang + dth;
        x = x + 10 * cosd(ang);
        zz = zz + 10 * sind(ang);
        if zz < 0 || zz > 400, break; end
    end
end
figure; plot(ranges', zr2); title('SOFAR ray family'); xlabel('range m'); ylabel('z m');

% --- 40. Pekeris waveguide: method of images ---------------------------
zs = 5; zr = 20; Hw = 50; rr2 = linspace(1, 2000, 400); f = 200;
lamb = c / f;
k = 2 * pi / lamb;
pTot = zeros(1, numel(rr2));
for m = -20:20
    if m == 0
        dzs = zs;
    elseif m > 0
        dzs = 2*m*Hw - zs;
    else
        dzs = -2*m*Hw - zs;
    end
    dzr2 = (zr - dzs);
    rr3 = sqrt(rr2.^2 + dzr2.^2) + 1e-6;
    if m >= 0, coef = 1; else, coef = -1; end   % surface V=-1
    pTot = pTot + coef * exp(-1i * k * rr3) ./ rr3;
end
figure; plot(rr2, 20*log10(abs(pTot))); title('Pekeris waveguide, 200 Hz');
xlabel('range m'); ylabel('|p| dB'); grid on;
