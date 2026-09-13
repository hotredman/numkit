% MUSIC: superresolution
% Two targets inside the main beam
% Pseudo-spectrum P(θ) = 1 / (noise-subspace energy of the steering
% vector): peaks where the steering vector is orthogonal to the noise
% subspace, i.e. AT the source angles.

sep = 8;
theta = -30:0.2:30;
t1 = -sep/2; t2 = sep/2;
N = 8;
a2v = exp(-1i*pi*sind(t1)*(0:N-1));   % source-1 steering vector
a3v = exp(-1i*pi*sind(t2)*(0:N-1));   % source-2 steering vector
g = sum(conj(a3v).*a2v);              % Gram cross-term of the 2 sources
detG = N^2 - abs(g)^2;                % Gram determinant
Pm = zeros(1, numel(theta));
for k = 1:numel(theta)
    a1 = exp(-1i*pi*sind(theta(k))*(0:N-1));
    r1 = sum(conj(a1).*a2v);          % correlation with source 1
    r2 = sum(conj(a1).*a3v);          % correlation with source 2
    % Exact signal-subspace projection energy (2x2 projector)
    eSig = (N*abs(r1)^2 - 2*real(conj(g)*r1*conj(r2)) + N*abs(r2)^2) / detG;
    eNoise = max(1e-6, N - eSig);     % noise-subspace energy (>= 0)
    Pm(k) = 1 / eNoise;
end
patDb = 10*log10(Pm / max(Pm));       % dB, 0 dB at the peaks
figure;
plot(theta, patDb);
title('MUSIC pseudo-spectrum'); xlabel('\theta deg'); ylabel('dB'); grid on;

%{
@play
{
 "title": "MUSIC: superresolution",
 "groups": [
  {
   "name": "Targets",
   "controls": [
    {
     "var": "sep",
     "label": "Spacing °",
     "min": 1,
     "max": 20,
     "step": 1,
     "default": 8
    }
   ]
  }
 ]
}
%}
