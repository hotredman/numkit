% Плоская решётка (URA)
% Двумерная диаграмма сетки Nx×Ny

Nx = 8; Ny = 8; thetaScan = 20;
thv = -90:5:90; phv = 0:5:90;
AFp = zeros(numel(phv), numel(thv));
for ip = 1:numel(phv)
    for it = 1:numel(thv)
        s = 0;
        for ix = 1:Nx
            for iy = 1:Ny
                s = s + exp(1i*pi*((ix-1)*sind(thv(it))*cosd(phv(ip)) + (iy-1)*sind(thv(it))*sind(phv(ip)) - (ix-1)*sind(thetaScan)*cosd(phv(ip))));
            end
        end
        AFp(ip,it) = abs(s)/(Nx*Ny);
    end
end
figure;
imagesc(thv, phv, AFp);
colormap('jet');
title('URA |AF(\theta,\phi)|');
xlabel('\theta deg'); ylabel('\phi deg');

%{
@play
{
 "title": "Плоская решётка (URA)",
 "groups": [
  {
   "name": "Сетка",
   "controls": [
    {
     "var": "Nx",
     "label": "Nx",
     "min": 4,
     "max": 16,
     "step": 2,
     "default": 8
    },
    {
     "var": "Ny",
     "label": "Ny",
     "min": 4,
     "max": 16,
     "step": 2,
     "default": 8
    },
    {
     "var": "thetaScan",
     "label": "θ скан",
     "min": -60,
     "max": 60,
     "step": 5,
     "default": 20
    }
   ]
  }
 ]
}
%}
