clear
import compat.*

% tonemap — Ward HDR → LDR for display.

HDR = zeros(8, 8, 3);
for c = 1:3
    for r = 1:8
        for cc = 1:8
            HDR(r, cc, c) = 0.001 + (r * cc * c) / 100;
        end
    end
end

fprintf('=== default RGB ===\n');
R = tonemap(HDR);
fprintf('R(4,4,:) = [%d %d %d] (expect [225 255 255])  class=%s\n', ...
    R(4,4,1), R(4,4,2), R(4,4,3), class(R));

fprintf('\n=== AdjustLightness [0.1 0.9] ===\n');
R = tonemap(HDR, 'AdjustLightness', [0.1 0.9]);
fprintf('R(4,4,:) = [%d %d %d] (expect [225 255 255])\n', R(4,4,1), R(4,4,2), R(4,4,3));

fprintf('\n=== AdjustSaturation = 2 ===\n');
R = tonemap(HDR, 'AdjustSaturation', 2);
fprintf('R(4,4,:) = [%d %d %d] (expect [186 255 255])\n', R(4,4,1), R(4,4,2), R(4,4,3));

fprintf('\n=== grayscale path ===\n');
HDRg = zeros(8, 8);
for r = 1:8, for cc = 1:8, HDRg(r, cc) = 0.001 + (r * cc) / 100; end, end
R = tonemap(HDRg);
fprintf('R(4,4) = %d (expect 255)  class=%s\n', R(4,4), class(R));

fprintf('\n=== all-zero HDR ===\n');
R = tonemap(zeros(8, 8, 3));
fprintf('max(R(:)) = %d (expect 0)\n', max(R(:)));
