%========================================================================
% SECTION B — UNDERWATER ACOUSTICS (formulas implemented inline; the
% literature equations are simple polynomials/sums — no toolbox needed)
%========================================================================

% --- 22. Mackenzie (1981) sound speed --------------------------------
T = 2:1:30; S = 35; D = 10;
c = 1448.96 + 4.591*T - 5.304e-2*T.^2 + 2.374e-4*T.^3 + 1.340*(S-35) ...
    + 1.630e-2*D + 1.675e-7*D.^2 - 1.025e-2*T*(S-35) - 7.139e-13*T.*D.^3;
figure; plot(T, c); title('Mackenzie 1981 sound speed'); xlabel('T \circC'); ylabel('c m/s'); grid on;

% --- 23. Del Grosso (1974) -------------------------------------------
cDG = 1402.392 + 4.63573*T - 3.5885e-2*T.^2 + 1.59964e-4*T.^3 ...
      - 1.3904e-6*T.^4 + 1.33428*(S-35) + 1.60272e-2*D ...
      + 8.34076e-7*D.^3 - 1.13083e-2*T.*(S-35);
figure; plot(T, cDG); title('Del Grosso 1974'); xlabel('T \circC'); ylabel('c m/s'); grid on;

% --- 24. UNESCO / Chen-Millero (1983) --------------------------------
cUN = 1449.2 + 4.6*T - 5.48e-2*T.^2 + 2.91e-4*T.^3 + 1.34*(S-35) ...
      + 1.60e-2*D + 1.6e-7*D.^2 - 1.02e-2*T.*(S-35);
figure; plot(T, cUN); title('UNESCO 1983'); xlabel('T \circC'); ylabel('c m/s'); grid on;

% --- 25. Coppens (1981) ----------------------------------------------
cCp = 1449.05 + 4.57*T - 5.21e-2*T.^2 + 2.31e-4*T.^3 + 1.33*(S-35) ...
      - 1.10e-2*T.*(S-35) + 1.655e-2*D;
figure; plot(T, cCp); title('Coppens 1981'); xlabel('T \circC'); ylabel('c m/s'); grid on;

% --- 26. Comparison ----------------------------------------------------
figure; plot(T, c - cDG, T, c - cUN, T, c - cCp);
legend('Mackenzie-DG','Mackenzie-UNESCO','Mackenzie-Coppens');
title('\Delta c between models'); grid on;

% --- 27. Thorp absorption ---------------------------------------------
f = logspace(1, 4.7, 200);               % 10 Hz .. 50 kHz
aThorp = 0.11 * f.^2 ./ (1 + f.^2) + 44 * f.^2 ./ (4100 + f.^2) ...
       + 2.75e-4 * f.^2 + 0.003;
figure; loglog(f, aThorp); title('Thorp absorption \alpha(f) dB/km');
xlabel('f Hz'); grid on;

% --- 28. Francois-Garrison (simplified boron + MgSO4 + water) ---------
pH = 8; Tkg = 10; Sg = 35; Dk = 1;
f1 = 2.8*sqrt(Sg/35)*10^((Tkg - 2)/43);        % boron relaxation
aB = 0.11 * f.^2 * f1 ./ (f1^2 + f.^2) * 1e-3; % dB/km scale
f2 = 62 * 10^((Tkg + 30)/100);                 % MgSO4 relaxation
aMg = 0.7 * f2 * f.^2 / (f2^2 + f.^2) * 1e-3;
aW  = 2.9e-2 * f.^2 / 1e3;
figure; loglog(f, aB + aMg + aW); title('Francois-Garrison \alpha(f)'); grid on;

% --- 29. Ainslie-McColm (simplified) ----------------------------------
aAM = 0.106 * f.^2 * f1 ./ (f1^2 + f.^2) * 1e-3 ...
    + 0.6 * f2 * f.^2 / (f2^2 + f.^2) * 1e-3 + 2.75e-4 * f.^2;
figure; loglog(f, aAM); title('Ainslie-McColm \alpha(f)'); grid on;

% --- 30. Transmission loss --------------------------------------------
r = linspace(10, 10000, 200); kalpha = 0.07;  % dB/km @ 3.5 kHz
TLsph = 20*log10(r) + kalpha * r / 1000;
TLcyl = 10*log10(r) + kalpha * r / 1000;
figure; semilogx(r, TLsph, r, TLcyl); legend('spherical 20\log r','cylindrical 10\log r');
title('Transmission loss'); xlabel('r m'); ylabel('dB'); grid on;

% --- 31. Wenz ambient noise -------------------------------------------
fw = logspace(1, 5, 200);
NLship  = 44 - 17*log10(fw/100);          % shipping (50 Hz corner)
NLwind  = 33 - 25*log10(fw/500);          % wind-driven
NLtherm = -15 + 20*log10(fw);             % thermal
figure; loglog(fw, NLship, fw, NLwind, fw, NLtherm);
legend('shipping','wind/sea state','thermal');
title('Wenz ambient noise spectra'); xlabel('f Hz'); ylabel('dB re 1\muPa'); grid on;

% --- 32. Optimal sonar frequency ---------------------------------------
Rk = 5;                                   % km
figure; semilogx(f, -aThorp * 2 * Rk);   % two-way loss vs f at 5 km
title('Two-way absorption loss @ 5 km'); xlabel('f Hz'); ylabel('-2\alphaR dB'); grid on;

% --- 33. Active sonar equation -----------------------------------------
SL = 220; TS = 10; NL = 60; DI = 20; DT = 5;
TLok = (SL + TS - NL - DI + DT) / 2;      % max one-way TL allowed
rOK = 10.^((TLok - kalpha * 0) / 20);      % no-absorption range scale
figure; semilogx(r, TLsph); hold on; plot([rOK rOK], [40 120], 'r--');
title('Active sonar: TL budget vs range'); xlabel('r m'); ylabel('TL dB'); grid on;

% --- 34. Passive sonar equation -----------------------------------------
SLp = 180; TLmax = SLp - (NL - DI + DT);
figure; semilogx(r, TLcyl); hold on; plot([r r(end)], [TLmax TLmax], 'r--');
title('Passive sonar: TL budget'); xlabel('r m'); ylabel('TL dB'); grid on;
