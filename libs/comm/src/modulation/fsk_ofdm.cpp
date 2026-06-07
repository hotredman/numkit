// libs/comm/src/modulation/fsk_ofdm.cpp

#include <numkit/comm/modulation/fsk_ofdm.hpp>

#include <numkit/signal/transforms/fft.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::comm {

namespace {

using Cd = std::complex<double>;

inline int to_gray(int b) { return b ^ (b >> 1); }
inline int from_gray(int g) {
    int b = g;
    while (g >>= 1) b ^= g;
    return b;
}

inline int symbol_to_freq_idx(int s, int M, const std::string &order) {
    return (order == "bin") ? s : to_gray(s);
}
inline int freq_idx_to_symbol(int k, int M, const std::string &order) {
    return (order == "bin") ? k : from_gray(k);
}

} // anonymous

Value fskmod(const Value &x, int M, double freq_sep, int nsamp,
             double fs, const std::string &phase_continuity,
             const std::string &symbol_order,
             std::pmr::memory_resource *mr)
{
    if (M < 2)
        throw Error("fskmod: M must be ≥ 2", 0, 0, "fskmod", "",
                    "numkit:fskmod:badM");
    if (freq_sep <= 0.0 || nsamp <= 0 || fs <= 0.0)
        throw Error("fskmod: freq_sep / nsamp / fs must be positive",
                    0, 0, "fskmod", "", "numkit:fskmod:badparam");

    const size_t Nsym = x.numel();
    const size_t Nout = Nsym * (size_t)nsamp;
    Value out = Value::matrix(Nout, 1, ValueType::COMPLEX, mr);
    if (Nout == 0) return out;
    Cd *od = out.complexDataMut();

    const bool cont = (phase_continuity == "cont");

    double phase = 0.0;
    for (size_t s = 0; s < Nsym; ++s) {
        const int sym = (int)x.elemAsDouble(s);
        const int k = symbol_to_freq_idx(sym, M, symbol_order);
        // Tone frequency, centered around 0 (symmetric tones).
        const double f = (double(k) - 0.5 * (M - 1)) * freq_sep;
        const double dphi = 2.0 * M_PI * f / fs;
        for (int n = 0; n < nsamp; ++n) {
            if (cont) {
                od[s * nsamp + (size_t)n] = Cd(std::cos(phase), std::sin(phase));
                phase += dphi;
            } else {
                const double th = 2.0 * M_PI * f * double(n) / fs;
                od[s * nsamp + (size_t)n] = Cd(std::cos(th), std::sin(th));
            }
        }
    }
    return out;
}

Value fskdemod(const Value &y, int M, double freq_sep, int nsamp,
               double fs, const std::string &symbol_order,
               std::pmr::memory_resource *mr)
{
    if (M < 2)
        throw Error("fskdemod: M must be ≥ 2", 0, 0, "fskdemod", "",
                    "numkit:fskdemod:badM");
    if (freq_sep <= 0.0 || nsamp <= 0 || fs <= 0.0)
        throw Error("fskdemod: freq_sep / nsamp / fs must be positive",
                    0, 0, "fskdemod", "", "numkit:fskdemod:badparam");

    const size_t Nsamps = y.numel();
    const size_t Nsym = Nsamps / (size_t)nsamp;
    Value out = Value::matrix(Nsym, 1, ValueType::DOUBLE, mr);
    if (Nsym == 0) return out;
    double *od = out.doubleDataMut();

    // Pre-compute reference tone phasors.
    std::vector<std::vector<Cd>> ref((size_t)M, std::vector<Cd>((size_t)nsamp));
    for (int k = 0; k < M; ++k) {
        const double f = (double(k) - 0.5 * (M - 1)) * freq_sep;
        const double dphi = 2.0 * M_PI * f / fs;
        for (int n = 0; n < nsamp; ++n) {
            const double th = dphi * double(n);
            ref[(size_t)k][(size_t)n] = Cd(std::cos(th), std::sin(th));
        }
    }

    for (size_t s = 0; s < Nsym; ++s) {
        // Correlate the segment against each candidate tone (conjugate
        // multiply + sum, take magnitude).
        int bestK = 0;
        double bestE = -1.0;
        for (int k = 0; k < M; ++k) {
            Cd acc(0.0, 0.0);
            for (int n = 0; n < nsamp; ++n) {
                Cd yi;
                if (y.type() == ValueType::COMPLEX)
                    yi = y.complexData()[s * nsamp + (size_t)n];
                else
                    yi = Cd(y.elemAsDouble(s * nsamp + (size_t)n), 0.0);
                acc += yi * std::conj(ref[(size_t)k][(size_t)n]);
            }
            const double E = std::norm(acc);
            if (E > bestE) { bestE = E; bestK = k; }
        }
        od[s] = double(freq_idx_to_symbol(bestK, M, symbol_order));
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// ofdmmod / ofdmdemod
// ════════════════════════════════════════════════════════════════════

Value ofdmmod(const Value &in, int nfft, int cplen,
              std::pmr::memory_resource *mr)
{
    if (nfft <= 0 || cplen < 0)
        throw Error("ofdmmod: bad nfft / cplen", 0, 0, "ofdmmod", "",
                    "numkit:ofdmmod:badparam");

    const size_t rows = in.dims().rows();
    const size_t cols = in.dims().cols();
    if ((int)rows != nfft)
        throw Error("ofdmmod: in must have nfft rows", 0, 0, "ofdmmod", "",
                    "numkit:ofdmmod:size");
    const size_t Nsym = cols;
    const size_t out_per_sym = (size_t)(nfft + cplen);
    Value out = Value::matrix(out_per_sym * Nsym, 1, ValueType::COMPLEX, mr);
    if (Nsym == 0) return out;
    Cd *od = out.complexDataMut();

    // For each column, take IFFT and prepend the cyclic prefix.
    for (size_t s = 0; s < Nsym; ++s) {
        // Extract column s into a 1×nfft Value, ifft along dim 1.
        Value col = Value::matrix((size_t)nfft, 1, ValueType::COMPLEX, mr);
        Cd *cd = col.complexDataMut();
        for (int i = 0; i < nfft; ++i) {
            if (in.type() == ValueType::COMPLEX)
                cd[i] = in.complexData()[s * (size_t)nfft + (size_t)i];
            else
                cd[i] = Cd(in.elemAsDouble(s * (size_t)nfft + (size_t)i), 0.0);
        }
        Value tx = ::numkit::signal::ifft(col, -1, 1, mr);

        Cd *txd = nullptr;
        std::vector<Cd> tmp;
        if (tx.type() == ValueType::COMPLEX) {
            txd = const_cast<Cd*>(tx.complexData());
        } else {
            tmp.resize((size_t)nfft);
            for (int i = 0; i < nfft; ++i) tmp[(size_t)i] = Cd(tx.elemAsDouble(i), 0.0);
            txd = tmp.data();
        }

        // CP = last cplen samples of the IFFT output.
        for (int i = 0; i < cplen; ++i)
            od[s * out_per_sym + (size_t)i] = txd[(size_t)nfft - (size_t)cplen + (size_t)i];
        // Body = full IFFT.
        for (int i = 0; i < nfft; ++i)
            od[s * out_per_sym + (size_t)cplen + (size_t)i] = txd[(size_t)i];
    }
    return out;
}

Value ofdmdemod(const Value &in, int nfft, int cplen, int symoffset,
                std::pmr::memory_resource *mr)
{
    if (nfft <= 0 || cplen < 0)
        throw Error("ofdmdemod: bad nfft / cplen", 0, 0, "ofdmdemod", "",
                    "numkit:ofdmdemod:badparam");
    if (symoffset < 0)         symoffset = cplen;
    if (symoffset > cplen)     symoffset = cplen;

    const size_t out_per_sym = (size_t)(nfft + cplen);
    const size_t Nsamps = in.numel();
    if (Nsamps % out_per_sym != 0)
        throw Error("ofdmdemod: input length must be a multiple of (nfft + cplen)",
                    0, 0, "ofdmdemod", "", "numkit:ofdmdemod:size");
    const size_t Nsym = Nsamps / out_per_sym;

    Value out = Value::matrix((size_t)nfft, Nsym, ValueType::COMPLEX, mr);
    Cd *od = out.complexDataMut();

    for (size_t s = 0; s < Nsym; ++s) {
        // Drop the CP region (size symoffset, default cplen).
        Value col = Value::matrix((size_t)nfft, 1, ValueType::COMPLEX, mr);
        Cd *cd = col.complexDataMut();
        for (int i = 0; i < nfft; ++i) {
            const size_t src = s * out_per_sym + (size_t)symoffset + (size_t)i;
            if (in.type() == ValueType::COMPLEX) cd[i] = in.complexData()[src];
            else                                  cd[i] = Cd(in.elemAsDouble(src), 0.0);
        }
        Value rx = ::numkit::signal::fft(col, -1, 1, mr);
        // Pack into the output column.
        const Cd *rxd = rx.complexData();
        for (int i = 0; i < nfft; ++i)
            od[s * (size_t)nfft + (size_t)i] = rxd[i];
    }
    return out;
}

} // namespace numkit::comm
