% Hamming window weighting
% Sidelobe reduction at the cost of beamwidth

N = 16;
w = 0.54 - 0.46*cos(2*pi*(0:N-1)/(N-1));   % Hamming weights
wsum = sum(w);                             % normalizer: unity main-beam gain
theta = -90:0.2:90;
AF = zeros(1, numel(theta));
for k = 1:N
    AF = AF + w(k)*exp(1i*pi*(k-1)*sind(theta));
end
patDb = 20*log10(abs(AF)/wsum + 1e-12);    % dB pattern, 0 dB at peak
figure;
plot(theta, patDb);
title('Hamming taper, dB'); xlabel('\theta deg'); ylabel('dB'); grid on;

%{
@play
{
 "title": "Hamming window",
 "groups": [
  {
   "name": "Array",
   "controls": [
    {
     "var": "N",
     "label": "Element count",
     "min": 8,
     "max": 64,
     "step": 2,
     "default": 16
    }
   ]
  }
 ]
}
%}
