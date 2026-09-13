% Волновод Пекериса
% Метод изображений: дно + поверхность

f = 200; Hw = 50; zs = 5; zr = 20; c = 1500;
k = 2*pi*f/c;
rr2 = linspace(1, 2000, 400);
pTot = zeros(1, numel(rr2));
for m = -20:20
    if m == 0
        dzs = zs;
    elseif m > 0
        dzs = 2*m*Hw - zs;
    else
        dzs = -2*m*Hw - zs;
    end
    dz = zr - dzs;
    rr3 = sqrt(rr2.^2 + dz.^2) + 1e-6;
    if m >= 0, coef = 1; else, coef = -1; end
    pTot = pTot + coef*exp(-1i*k*rr3)./rr3;
end
figure;
plot(rr2, 20*log10(abs(pTot)));
title('Pekeris waveguide 200 Hz'); xlabel('range м'); ylabel('|p| дБ'); grid on;

%{
@play
{
 "title": "Pekeris waveguide",
 "groups": [
  {
   "name": "Waveguide",
   "controls": [
    {
     "var": "f",
     "label": "f Hz",
     "min": 50,
     "max": 500,
     "step": 50,
     "default": 200
    },
    {
     "var": "Hw",
     "label": "Sea depth m",
     "min": 20,
     "max": 100,
     "step": 10,
     "default": 50
    }
   ]
  }
 ]
}
%}
