% Taylor weighting
% Falling sidelobes — the radar standard

N = 16;
sll = -30;
w = taylorwin(N, 4, sll);
wsum = sum(w);                       % normalizer: unity main-beam gain
theta = -90:0.2:90;
AF = zeros(1, numel(theta));
for k = 1:N
    AF = AF + w(k)*exp(1i*pi*(k-1)*sind(theta));
end
patDb = 20*log10(abs(AF)/wsum + 1e-12);   % dB pattern, 0 dB at peak
figure;
plot(theta, patDb);
title('Taylor taper'); xlabel('\theta deg'); ylabel('dB'); grid on;

%{
@play
{
 "title": "Taylor taper",
 "groups": [
  {
   "name": "Synthesis",
   "controls": [
    {
     "var": "N",
     "label": "N",
     "min": 8,
     "max": 32,
     "step": 2,
     "default": 16
    },
    {
     "var": "sll",
     "label": "SLL dB",
     "min": -40,
     "max": -15,
     "step": 5,
     "default": -30
    }
   ]
  }
 ]
}
%}
