% MUSIC: сверхразрешение
% Две цели в главном луче

sep = 8;
theta = -30:0.2:30;
t1 = -sep/2; t2 = sep/2;
% Simplified MUSIC pseudo-spectrum (two sources)
Pm = zeros(1, numel(theta));
N = 8;
for k = 1:numel(theta)
    a1 = exp(-1i*pi*sind(theta(k))*[0:N-1]);
    a2v = exp(-1i*pi*sind(t1)*[0:N-1]);
    a3v = exp(-1i*pi*sind(t2)*[0:N-1]);
    denom = abs(sum(conj(a1).*a2v))^2 + abs(sum(conj(a1).*a3v))^2;
    Pm(k) = 1/(1 + denom);
end
figure;
plot(theta, 10*log10(Pm/max(Pm)));
title('MUSIC pseudo-spectrum'); ylabel('dB'); grid on;

%{
@play
{
 "title": "MUSIC: сверхразрешение",
 "groups": [
  {
   "name": "Цели",
   "controls": [
    {
     "var": "sep",
     "label": "Разнос °",
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
