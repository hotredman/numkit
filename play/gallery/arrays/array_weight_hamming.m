% Окно Хэмминга
% Снижение боковых ценой ширины луча

N = 16;
theta = -90:0.2:90;
AF = zeros(1, numel(theta));
for k = 1:N
    w = 0.54 - 0.46*cos(2*pi*(k-1)/(N-1));
    AF = AF + w*exp(1i*pi*(k-1)*sind(theta));
end
figure;
plot(theta, 20*log10(abs(AF)/sum(0.54 - 0.46*cos(2*pi*(0:N-1)/(N-1))) + 1e-12));
title('Hamming taper, dB'); ylabel('dB'); grid on;

%{
@play
{
 "title": "Окно Хэмминга",
 "groups": [
  {
   "name": "Решётка",
   "controls": [
    {
     "var": "N",
     "label": "Число элементов",
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
