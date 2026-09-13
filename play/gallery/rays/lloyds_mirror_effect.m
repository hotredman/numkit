% Зеркало Ллойда
% Интерференция прямого и отражённого

f = 500; r = 1000; c = 1500;
z = linspace(1, 100, 300);
dp = 2*z*r./sqrt(r^2+z.^2);
I = 4*cos(pi*dp*f/c).^2;
figure;
plot(z, 20*log10(I));
title('Lloyd mirror: level vs depth');
xlabel('z м'); ylabel('отн. дБ'); grid on;

%{
@play
{
 "title": "Lloyd's mirror",
 "groups": [
  {
   "name": "Parameters",
   "controls": [
    {
     "var": "f",
     "label": "f Hz",
     "min": 100,
     "max": 2000,
     "step": 100,
     "default": 500
    },
    {
     "var": "r",
     "label": "Range m",
     "min": 100,
     "max": 5000,
     "step": 100,
     "default": 1000
    }
   ]
  }
 ]
}
%}
