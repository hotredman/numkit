% Uniform weighting
% Maximum directivity; first sidelobe at -13.3 dB

N = 16;
theta = -90:0.2:90;
AF = zeros(1, numel(theta));
for k = 1:N
    AF = AF + exp(1i*pi*(k-1)*sind(theta));
end
pat = abs(AF) / N;                       % normalized beam pattern (peak 1)
patDb = 20*log10(pat + 1e-12);           % dB pattern, 0 dB at peak
figure;
plot(theta, patDb);
title('Uniform weight, dB'); xlabel('\theta deg'); ylabel('dB'); grid on;

%{
@play
{
 "title": "Uniform window",
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
