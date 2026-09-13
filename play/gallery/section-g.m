%========================================================================
% SECTION G — WAVE PHYSICS & OPTICS
%========================================================================

% --- 64. Thin-film AR coating (TMM) ---------------------------------------
n0 = 1.0; nf = 1.38; ns = 1.5;     % air / MgF2 / glass
df = 100e-9;                        % 100 nm film (lambda/4 @ 550)
lam = linspace(380, 780, 400);
delta = 2*pi*nf*df ./ lam;
r01 = (n0 - nf)/(n0 + nf); r12 = (nf - ns)/(nf + ns);
R = abs( (r01 + r12*exp(-2i*delta)) ./ (1 + r01*r12*exp(-2i*delta)) ).^2;
figure; plot(lam, R*100); title('AR coating R(\lambda), MgF_2 on glass');
xlabel('\lambda nm'); ylabel('R %'); grid on;

% --- 65. Airy disk ----------------------------------------------------------
u = linspace(1e-6, 15, 400);
airy = (2*besselj(1, u)./u).^2;
figure; plot(u, airy); title('Airy pattern [2J_1(u)/u]^2');
xlabel('u = \pi D \theta / \lambda'); ylabel('intensity'); grid on;
% Rayleigh criterion: first zero at u = 3.8317
figure; polarplot(linspace(0, 2*pi, 360), (2*besselj(1, 3.8317*sin(linspace(0,2*pi,360)/2)+1e-9)/max(1e-9,3.8317*sin(linspace(0,2*pi,360)/2)+1e-9)).^2);
title('Airy disk cross-section (polar)');

% --- 66. Damped harmonic resonance ------------------------------------------
wn = 2*pi*2;                       % 2 Hz natural
zv = [0.05 0.1 0.3 0.6];
wr = linspace(0.1, 2.3*wn, 400);
figure; hold on;
for k = 1:numel(zv)
    Hr = 1 ./ sqrt( (1 - (wr/wn).^2).^2 + (2*zv(k)*wr/wn).^2 );
    plot(wr/wn, Hr);
end
hold off; title('Resonance amplitude vs \zeta');
xlabel('\omega/\omega_n'); legend('\zeta=0.05','\zeta=0.1','\zeta=0.3','\zeta=0.6'); grid on;

% --- 67. Lorenz attractor -----------------------------------------------------
sigma = 10; rho = 28; beta = 8/3;
fL = @(t, s) [sigma*(s(2)-s(1)); s(1)*(rho-s(3))-s(2); s(1)*s(2)-beta*s(3)];
[tL, yL] = ode45(fL, [0 20], [1 1 1]);
figure; plot3(yL(:,1), yL(:,2), yL(:,3));
title('Lorenz attractor (\rho=28)'); xlabel('x'); ylabel('y'); zlabel('z');
grid on;
% bifurcation sweep: rho from 10 to 40, plot z-max of steady state
rhoS = 10:2:40;
zMax = zeros(size(rhoS));
for k = 1:numel(rhoS)
    rr = rhoS(k);
    fK = @(t, s) [sigma*(s(2)-s(1)); s(1)*(rr-s(3))-s(2); s(1)*s(2)-beta*s(3)];
    [~, yK] = ode45(fK, [0 30], [1 1 1]);
    tail = yK(round(end/2):end, 3);
    zMax(k) = max(tail);
end
figure; plot(rhoS, zMax, 'o-'); title('Bifurcation: max z vs \rho');
xlabel('\rho'); grid on;
