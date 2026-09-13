% Гексагональная решётка
% Плотная упаковка элементов

rings = 2;
pos = [0 0];
for ring = 1:rings
    for s = 0:5
        an = pi/3*s;
        for m = 1:ring
            rr = m/ring;
            pos(end+1,:) = ring*[cos(an) sin(an)] + rr*[cos(an+pi/2) sin(an+pi/2)];
        end
    end
end
ph2 = 0:1:180;
AFh = zeros(1, numel(ph2));
for ip = 1:numel(ph2)
    th0 = ph2(ip)*pi/180;
    s = 0;
    for n = 1:size(pos,1)
        s = s + exp(1i*2*pi*(pos(n,1)*cos(th0) + pos(n,2)*sin(th0)));
    end
    AFh(ip) = abs(s)/size(pos,1);
end
figure;
plot(ph2, 20*log10(AFh + 1e-12));
title('Hexagonal array, dB'); ylabel('|AF| dB'); grid on;

%{
@play
{
 "title": "Hexagonal array",
 "groups": [
  {
   "name": "Geometry",
   "controls": [
    {
     "var": "rings",
     "label": "Ring count",
     "min": 1,
     "max": 4,
     "step": 1,
     "default": 2
    }
   ]
  }
 ]
}
%}
