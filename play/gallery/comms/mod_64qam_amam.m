% 64-QAM + AM/AM
% Компрессия внешних точек

alpha = 1.3;
step6 = 2*(0:7)/7 - 1;
[Ix6, Iy6] = meshgrid(step6, step6);
Iq6 = Ix6(:) + 1i*Iy6(:);
comp = sign(real(Iq6)).*abs(real(Iq6)).^alpha + 1i*sign(imag(Iq6)).*abs(imag(Iq6)).^alpha;
figure;
plot(real(Iq6), imag(Iq6), '.', real(comp), imag(comp), 'r.');
legend('ideal','compressed'); title('64-QAM AM/AM'); grid on;

%{
@play
{
 "title": "64-QAM + AM/AM",
 "groups": [
  {
   "name": "Amplifier",
   "controls": [
    {
     "var": "alpha",
     "label": "Order",
     "min": 0.8,
     "max": 1.5,
     "step": 0.05,
     "default": 1.3
    }
   ]
  }
 ]
}
%}
