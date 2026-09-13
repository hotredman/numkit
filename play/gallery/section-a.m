%========================================================================
% SECTION A — ANTENNA & ACOUSTIC ARRAYS (21 scripts, batched in one
% generator file; each numbered script below becomes a gallery entry
% via its own @play manifest when split — during authoring they run
% top-to-bottom in one engine session each).
%========================================================================

% ── A.1 GEOMETRIES ──────────────────────────────────────────────────

% --- 1. ULA: grating-lobe onset vs d/lambda -------------------------
d = 0.5; N = 16; theta0 = 20;
theta = -90:0.2:90;
ph = 2*pi*d*(sind(theta) - sind(theta0));
AF = zeros(1,numel(theta));
for k = 1:N, AF = AF + exp(1i*(k-1)*ph); end
figure; plot(theta, abs(AF)/N); title('ULA beam, d=0.5'); grid on;

d = 1.2;
ph = 2*pi*d*(sind(theta) - sind(theta0));
AF2 = zeros(1,numel(theta));
for k = 1:N, AF2 = AF2 + exp(1i*(k-1)*ph); end
figure; plot(theta, abs(AF2)/N); title('ULA d=1.2: grating lobes'); grid on;

% --- 2. UCA: ring array ---------------------------------------------
R = 1.0; Ne = 24; f = 1;
th = linspace(0, 2*pi, 361);
AFu = zeros(1,numel(th));
for n = 1:Ne
  an = 2*pi*(n-1)/Ne;
  AFu = AFu + exp(1i*2*pi*R*(cos(an).*cos(th) + sin(an).*sin(th)));
end
figure; polarplot(th, abs(AFu)/Ne); title('UCA ring pattern');

% --- 3. URA planar: 2-D grid cut --------------------------------------
Nx = 8; Ny = 8;
thv = -90:5:90; phv = 0:5:90;
AFp = zeros(numel(phv), numel(thv));
for ip = 1:numel(phv)
  for it = 1:numel(thv)
    s = 0;
    for ix = 1:Nx
      for iy = 1:Ny
        s = s + exp(1i*pi*( (ix-1)*sind(thv(it))*cosd(phv(ip)) + ...
                            (iy-1)*sind(thv(it))*sind(phv(ip)) ));
      end
    end
    AFp(ip,it) = abs(s)/(Nx*Ny);
  end
end
figure; imagesc(thv, phv, AFp); colormap('jet'); colorbar;
title('URA planar |AF(\theta,\phi)|'); xlabel('\theta'); ylabel('\phi');

% --- 4. Hexagonal pack: denser aperture, lower sidelobes -------------
ph2 = 0:1:180;
AFh = zeros(1,numel(ph2));
pos = [0 0];
for ring = 1:2
  for s = 0:5
    an = pi/3*s;
    for m = 1:ring
      rr = m/ring;
      pos(end+1,:) = ring*[cos(an) sin(an)] + rr*[cos(an+pi/2) sin(an+pi/2)];
    end
  end
end
for ip = 1:numel(ph2)
  th0 = ph2(ip)*pi/180;
  s = 0;
  for n = 1:size(pos,1)
    s = s + exp(1i*2*pi*( pos(n,1)*cos(th0) + pos(n,2)*sin(th0) ));
  end
  AFh(ip) = abs(s)/size(pos,1);
end
figure; plot(ph2, 20*log10(AFh + 1e-12)); title('Hexagonal array, dB');
ylabel('|AF| dB'); grid on;

% --- 5. Conformal cylinder: sector switching -------------------------
Nc = 32; Rc = 1.0; sector = 120;
th = -90:1:90;
AFc = zeros(1,numel(th));
for n = 1:Nc
  an = -180 + 360*(n-1)/Nc;
  if abs(an) <= sector/2
    AFc = AFc + exp(1i*2*pi*Rc*(cosd(an).*cosd(th) + sind(an).*sind(th)));
  end
end
figure; polarplot(th*pi/180, abs(AFc)/sum(abs(AFc)>0)); title('Cylinder conformal, 120\circ sector');

% --- 6. Sparse MRA: minimum-redundancy aperture ----------------------
mra = [0 1 3 6 8 11 13 16];  % Golomb-ruler-like positions (elements)
theta = -90:0.5:90;
AFm = zeros(1,numel(theta));
for n = 1:numel(mra)
  AFm = AFm + exp(1i*pi*mra(n)*sind(theta));
end
figure; plot(theta, 20*log10(abs(AFm)/numel(mra) + 1e-12));
title('MRA sparse array'); ylabel('dB'); grid on;

% --- 7. Spherical cap: hemispheric coverage --------------------------
Ns = 5; % rings
th = 0:2:180;
AFs = zeros(1,numel(th));
for ring = 0:Ns-1
  elr = ring * 30;            % ring elevation deg
  cnt = max(4, round(12*cosd(elr)));
  for m = 1:cnt
    az = 360*(m-1)/cnt;
    for it = 1:numel(th)
      u = cosd(th(it))*cosd(az) + sind(th(it))*sind(az);
      AFs(it) = AFs(it) + exp(1i*pi*u*sind(elr));
    end
  end
end
figure; plot(th, abs(AFs)/max(abs(AFs))); title('Spherical cap coverage');

% ── A.2 ELEMENT PATTERNS ────────────────────────────────────────────

% --- 8. Isotropic -----------------------------------------------------
figure; polarplot(linspace(0,2*pi,361), ones(1,361)); title('Isotropic F(\theta)=1');

% --- 9. Short dipole F=cos(theta) -------------------------------------
th = linspace(0, 2*pi, 361);
figure; polarplot(th, abs(cos(th))); title('Short dipole F=\cos\theta');

% --- 10. Half-wave dipole ---------------------------------------------
figure; polarplot(th, abs(cos(pi/2*cos(th))./max(1e-9,sin(th))));
title('Half-wave dipole');

% --- 11. Cardioid ------------------------------------------------------
figure; polarplot(th, abs(1 + cos(th))); title('Cardioid 1+\cos\theta');

% --- 12. Baffled piston 2*J1(x)/x --------------------------------------
u = linspace(1e-6, 40, 721);
pat = 2*besselj(1, u)./u;
figure; plot(u, pat); title('Baffled piston 2J_1(x)/x'); grid on;
