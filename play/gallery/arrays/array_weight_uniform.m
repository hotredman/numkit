% Равномерное окно
% Максимальный КНД; первые боковые −13.3 дБ

N = 16;
theta = -90:0.2:90;
AF = zeros(1, numel(theta));
for k = 1:N
    AF = AF + exp(1i*pi*(k-1)*sind(theta));
end
figure;
plot(theta, 20*log10(abs(AF)/N + 1e-12));
title('Uniform weight, dB'); ylabel('dB'); grid on;

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
