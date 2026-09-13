% OFDM: ортогональность и ICI
% sinc-поднесущие, доплеровский сдвиг

dop = 20; Nc = 8;
fs2 = 4000;
to = 0:1/fs2:2e-3;
base = 1e3;
Sub = zeros(Nc, numel(to));
for k = 1:Nc
    Sub(k,:) = sin(2*pi*(base + (k-1)*100)*to);
end
Sici = sin(2*pi*(base + (0:Nc-1)*100 + dop)'*to);
figure;
plot(to, Sub(1,:) + Sub(2,:));
title('OFDM: orthogonal sum'); grid on;
figure;
plot(to, Sici(1,:) + Sici(2,:));
title('OFDM: Doppler ICI'); grid on;

%{
@play
{
 "title": "OFDM: orthogonality and ICI",
 "groups": [
  {
   "name": "Channel",
   "controls": [
    {
     "var": "dop",
     "label": "Doppler Hz",
     "min": 0,
     "max": 50,
     "step": 5,
     "default": 20
    }
   ]
  }
 ]
}
%}
