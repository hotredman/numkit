% Годограф корней
% Полюса замкнутой системы vs K

K = 1;
denCL = [1 2 2 + K];
pr = roots(denCL);
figure;
plot(real(pr), imag(pr), 'x');
title('Closed-loop poles at K');
xlabel('Re'); ylabel('Im'); grid on;

%{
@play
{
 "title": "Годограф корней",
 "groups": [
  {
   "name": "Усиление",
   "controls": [
    {
     "var": "K",
     "label": "K",
     "min": 0.1,
     "max": 5,
     "step": 0.1,
     "default": 1
    }
   ]
  }
 ]
}
%}
