% ber_curve_bpsk.m
% Empirical bit-error-rate of BPSK over AWGN, swept across SNR.
% Compare with the theoretical curve  Pe = Q(sqrt(2 * Eb/N0)).

import compat.*;

clear
close all

rng(0);

EbN0_dB = 0:2:12;
N       = 200000;                       % bits per SNR point

ber_emp = zeros(size(EbN0_dB));
for i = 1:numel(EbN0_dB)
    bits = randi([0 1], N, 1);
    sym  = 2 * bits - 1;                % BPSK: 0→-1, 1→+1
    rx   = awgn(sym, EbN0_dB(i), 'measured');
    bits_hat = rx > 0;
    ber_emp(i) = mean(bits ~= bits_hat);
end

% Theoretical Q-function for BPSK.
ber_th = 0.5 * erfc(sqrt(10.^(EbN0_dB/10)));

fprintf('Eb/N0 (dB) :  empirical BER  /  theoretical BER\n');
for i = 1:numel(EbN0_dB)
    fprintf('   %5.1f      %10.3e   %10.3e\n', EbN0_dB(i), ber_emp(i), ber_th(i));
end

figure;
semilogy(EbN0_dB, ber_emp, 'bo-', EbN0_dB, ber_th, 'r--', 'LineWidth', 1.5);
grid on;
xlabel('E_b/N_0 (dB)'); ylabel('BER');
legend('empirical', 'theoretical Q(\surd 2E_b/N_0)', 'location', 'southwest');
title('BPSK BER over AWGN');
