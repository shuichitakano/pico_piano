/*
 * author : Shuichi TAKANO
 * since  : Thu Apr 18 2019 1:32:53
 */
#ifndef _59B3E3AF_7134_14C2_A887_B871570FF94C
#define _59B3E3AF_7134_14C2_A887_B871570FF94C

#include "fixed.h"
#include <array>
#include <assert.h>
#include <stdio.h>
#include <utility>

#include <pico/platform.h>

namespace physical_modeling_piano
{

    enum class BiquadFilterType
    {
        ALLPASS,
        LOWPASS,
        HIGHPASS,
        NOTCH,
    };

    namespace detail
    {

        float
        computePhaseDelay(int cn, const float *ca, const float *cb, float f, float Fs);
        float
        computeGroupDelay(int cn, const float *ca, const float *cb, float f, float Fs);

        void thirian(int cn, float *ca, float *cb, float D);

        void dumpFilter(const char *str, int n, const float *a, const float *b);

        void makeBiquadFilter(
            float *ca, float *cb, float f0, float fs, float Q, BiquadFilterType type);

        void makeLossFilter(
            float ca[2], float cb[2], float f0, float fs, float c1, float c3);

        void makeThirianDispersionFilter(float *ca, float *cb, float B, float f, int M);

    } // namespace detail

    ////
    template <size_t Size, class T = float>
    struct IIRFilterState
    {
        using Array = std::array<T, Size>;
        Array state{};

    public:
        void clear() { memset(state.data(), 0, sizeof(state)); }
        void clear(size_t n) { memset(state.data(), 0, sizeof(T) * n); }

        T *data() { return state.data(); }
    };

    ////

    template <size_t Size, class T = float>
    struct IIRFilterConstant
    {
        using Array = std::array<T, Size>;
        Array a;
        Array b;

    private:
        template <int I>
        struct I2T
        {
            static constexpr int value = I;
        };

    public:
        template <size_t N, class TV, class TH>
        inline TV __time_critical_func(_filter)(const TV &in, TH *__restrict__ history) const
        {
            static_assert(N + 1 <= Size, "too large kernel size.");

            // float out = history[0] + b[0] * in;
            // proc<N - 1>(in, out, history);
            // history[N - 1] = b[N] * in - a[N] * out;
            TV out;
            madd(out, history[0], b[0], in);
            proc(in, out, history, I2T<N - 1>());

            TH tmp;
            mul(tmp, b[N], in);
            nmsub(history[N - 1], tmp, a[N], out);
            return out;
        }

        template <size_t N, size_t M, class TV, class TH>
        __attribute__((always_inline)) TV __time_critical_func(filter)(const TV &in, IIRFilterState<M, TH> &state) const
        {
            static_assert(N <= M, "");
            return _filter<N>(in, state.data());
        }

        float computeGroupDelay(int N, float f, float Fs) const
        {
            assert(N + 1 <= Size);
            float ca[Size];
            float cb[Size];
            for (int i = 0; i < N + 1; ++i)
            {
                ca[i] = a[i];
                cb[i] = b[i];
            }
            return detail::computeGroupDelay(N, ca, cb, f, Fs);
        }

        void reset()
        {
            a[0] = 1.0f;
            b[0] = 1.0f;
            for (int i = 1; i < Size; ++i)
            {
                a[i] = 0;
                b[i] = 0;
            }
        }

        void copy(const float *sa, const float *sb, size_t size)
        {
            for (size_t i = 0; i < size; ++i)
            {
                a[i] = sa[i];
                b[i] = sb[i];
            }
        }

    protected:
        template <class TV, class TH, int I>
        __attribute__((always_inline)) void proc(const TV &in, const TV &out, TH *__restrict__ history, I2T<I>) const
        {
            proc(in, out, history, I2T<I - 1>());
            TH tmp;
            madd(tmp, history[I], b[I], in);
            nmsub(history[I - 1], tmp, a[I], out);
        }

        template <class TV, class TH>
        void
            __attribute__((always_inline)) proc(const TV &in, const TV &out, TH *__restrict__ history, I2T<0>) const
        {
        }
    };

    ////
    template <size_t N, class TC = float, class TH = float>
    class FixedSizeIIRFilter
    {
    public:
        using Constant = IIRFilterConstant<N + 1, TC>;
        using State = IIRFilterState<N, TH>;

    public:
        template <class TV>
        TV __time_critical_func(filter)(const TV &in, State &st) const
        {
            return constant_.template filter<N>(in, st);
        }

        float computeGroupDelay(float f, float Fs) const
        {
            return constant_.computeGroupDelay(N, f, Fs);
        }

        void reset() { constant_.reset(); }

        void copy(const float *sa, const float *sb)
        {
            constant_.copy(sa, sb, N + 1);
        }

        void clear(State &st) const { st.clear(); }

    protected:
        Constant &getConstant() { return constant_; }

    private:
        Constant constant_;
    };

    ////
    template <size_t N_MAX, class TC = float, class TH = float, class TV = float>
    class VariableSizeIIRFilter
    {
    public:
        using Constant = IIRFilterConstant<N_MAX + 1, TC>;
        using State = IIRFilterState<N_MAX, TH>;

        using FilterFunc = TV (Constant::*)(const TV &, State &) const;

    public:
        void setDim(size_t n)
        {
            assert(n >= 1);
            assert(n <= N_MAX);
            setDimImpl(n, std::make_index_sequence<N_MAX - 1>());
        }

        TV __time_critical_func(filter)(const TV &in, State &st) const
        {
            return (constant_.*filterFunc_)(in, st);
        }

        float computeGroupDelay(float f, float Fs) const
        {
            return constant_.computeGroupDelay(n_, f, Fs);
        }

        void reset() { constant_.reset(); }

        void clear(State &st) const { st.clear(n_); }

        void copy(const float *sa, const float *sb, size_t size)
        {
            constant_.copy(sa, sb, size);
        }

    protected:
        Constant &getConstant() { return constant_; }

    private:
        template <size_t... I>
        void setDimImpl(size_t n, std::index_sequence<I...>)
        {
            static FilterFunc funcTable[] = {
                &Constant::template filter<I + 1, N_MAX, TV, TH>...};

            n_ = n;
            filterFunc_ = funcTable[n - 1];
        }

    private:
        Constant constant_;
        FilterFunc filterFunc_;
        size_t n_ = 0;
    };

    ////
    template <class TC = float, class TH = float>
    class BiquadFilter : public FixedSizeIIRFilter<2, TC, TH>
    {
    public:
        void initialize(float f0, float fs, float Q, BiquadFilterType type)
        {
            float ca[3];
            float cb[3];
            detail::makeBiquadFilter(ca, cb, f0, fs, Q, type);
            this->copy(ca, cb);
        }
    };

////
#if 0
template <class TC = float, class TH = float>
class LossFilter : public FixedSizeIIRFilter<1, TC, TH>
{
public:
    void initialize(float f0, float fs, float c1, float c3)
    {
        float ca[2];
        float cb[2];
        detail::makeLossFilter(ca, cb, f0, fs, c1, c3);
        this->copy(ca, cb);
    }
};
#else
    template <class TC = float, class TH = float>
    class LossFilter
    {
        TC ma1_;
        TC b0_;

    public:
        struct State
        {
            TH h0;
        };

    public:
        void initialize(float f0, float fs, float c1, float c3)
        {
            float ca[2];
            float cb[2];
            detail::makeLossFilter(ca, cb, f0, fs, c1, c3);
            ma1_ = -ca[1];
            b0_ = cb[0];
        }

        template <class TV>
        TV __time_critical_func(filter)(const TV &in, State &st) const
        {
            TV out;
            madd(out, st.h0, b0_, in);
            mul(st.h0, ma1_, out);
            return out;
        }

        float computeGroupDelay(float f, float Fs) const
        {
            float ca[2];
            float cb[2];
            ca[0] = 1.0f;
            ca[1] = -ma1_;
            cb[0] = b0_;
            cb[1] = 0;
            return detail::computeGroupDelay(1, ca, cb, f, Fs);
        }

        void clear(State &st) const { st.h0 = 0; }
    };
#endif

    ////
    template <class TC = float, class TH = float>
    class ThirianDispersionFilter : public FixedSizeIIRFilter<2, TC, TH>
    {
    public:
        void initialize(float B, float f, int M)
        {
            float ca[3];
            float cb[3];
            detail::makeThirianDispersionFilter(ca, cb, B, f, M);
            this->copy(ca, cb);
        }
    };

    ////
    template <size_t N_MAX, class TC = float, class TH = float, class TV = float>
    class ThirianFilter : public VariableSizeIIRFilter<N_MAX, TC, TH, TV>
    {
    public:
        void initialize(float D, int N)
        {
            this->setDim(N);

            float ca[N_MAX + 1];
            float cb[N_MAX + 1];
            detail::thirian(N, ca, cb, D);
            this->copy(ca, cb, N + 1);
            detail::dumpFilter("thirian", N, ca, cb);
        }
    };

    ////
    template <class T>
    struct Filter
    {
        T constant;
        typename T::State state;

    public:
        template <class TV>
        TV __time_critical_func(filter)(const TV &in)
        {
            return constant.filter(in, state);
        }
    };

} // namespace physical_modeling_piano

#endif /* _59B3E3AF_7134_14C2_A887_B871570FF94C */
