/*
 * author : Shuichi TAKANO
 * since  : Thu Apr 25 2019 1:52:25
 */
#ifndef _4CAD8B76_C134_14C4_1746_4D5010DEF3DD
#define _4CAD8B76_C134_14C4_1746_4D5010DEF3DD

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <utility>

#include <pico/platform.h>

namespace physical_modeling_piano
{

    namespace detail
    {
        template <int LSHIFT,
                  class T,
                  std::enable_if_t<(LSHIFT > 0), std::nullptr_t> = nullptr>
        T
        shift(T v)
        {
            return v << LSHIFT;
        }

        template <int LSHIFT,
                  class T,
                  std::enable_if_t<(LSHIFT == 0), std::nullptr_t> = nullptr>
        T
        shift(T v)
        {
            return v;
        }

        template <int LSHIFT,
                  class T,
                  std::enable_if_t<(LSHIFT < 0), std::nullptr_t> = nullptr>
        T
        shift(T v)
        {
            return v >> (-LSHIFT);
        }

        template <class T>
        T
        dshift(T v, int s)
        {
            return s >= 0 ? v << s : v >> -s;
        }
    } // namespace detail

    template <class T, int LSHIFT>
    class FixedPoint
    {
        using self = FixedPoint;
        using value_type = T;
        T value_{};

        static constexpr float scale_ = LSHIFT > 0
                                            ? ((uint64_t)1 << LSHIFT)
                                            : 1.0f / ((uint64_t)1 << -LSHIFT);

    public:
        FixedPoint() = default;
        constexpr FixedPoint(const self &v) = default;

        template <int S2>
        constexpr FixedPoint(const FixedPoint<T, S2> &v)
        {
            *this = v;
        }

        constexpr FixedPoint(float v) { assign(v); }
        constexpr FixedPoint(int v) { assign(v); }

        constexpr self &operator=(const self &v) = default;

        template <int S2>
        self &operator=(const FixedPoint<T, S2> &v)
        {
            value_ = detail::shift<LSHIFT - S2>(v.get());
            return *this;
        }

        self &operator=(float v)
        {
            assign(v);
            return *this;
        }

        constexpr void assign(float v) { value_ = T(v * scale_ + 0.5f); }
        constexpr void assign(int v) { value_ = v << LSHIFT; }
        constexpr operator float() const { return value_ * (1.0f / scale_); }

        constexpr value_type get() const { return value_; }
        void set(T v) { value_ = v; }
    };

    template <int N>
    inline void
    shift(float &dst, float v)
    {
        dst = v * (N < 0 ? 1.0f / (1 << N) : 1 << N);
    }

    template <int N, class T, int S1, int S2>
    void
    shift(FixedPoint<T, S1> &dst, const FixedPoint<T, S2> &v)
    {
        dst.set(detail::shift<S1 - S2 + N>(v.get()));
    }

    inline bool
    isPlus(float v)
    {
        return v > 0;
    }

    template <class T, int S>
    bool
    isPlus(const FixedPoint<T, S> &v)
    {
        return v.get() > 0;
    }

    inline void
    neg(float &dst, float v)
    {
        dst = -v;
    }

    template <class T, int S>
    void
    neg(FixedPoint<T, S> &dst, const FixedPoint<T, S> &v)
    {
        dst.set(-v.get());
    }

    inline void
    clamp0(float &dst, float v)
    {
        dst = v > 0 ? v : 0;
    }

    template <class T, int S>
    void
    clamp0(FixedPoint<T, S> &dst, const FixedPoint<T, S> &v)
    {
        auto vv = v.get();
        dst.set(vv > 0 ? vv : 0);
    }

    inline void
    add(float &dst, float a, float b)
    {
        dst = a + b;
    }

    template <class T, int S>
    void
    add(FixedPoint<T, S> &dst, const FixedPoint<T, S> &a, const FixedPoint<T, S> &b)
    {
        dst.set(a.get() + b.get());
    }

    inline void
    sub(float &dst, float a, float b)
    {
        dst = a - b;
    }

    template <class T, int S>
    void
    sub(FixedPoint<T, S> &dst, const FixedPoint<T, S> &a, const FixedPoint<T, S> &b)
    {
        dst.set(a.get() - b.get());
    }

    inline void
    mul(float &dst, float a, float b)
    {
        dst = a * b;
    }

    template <int SD, class T, class T1, class T2, int S1, int S2>
    void
    mul(FixedPoint<T, SD> &dst,
        const FixedPoint<T1, S1> &a,
        const FixedPoint<T2, S2> &b)
    {
        dst.set(detail::shift<SD - S1 - S2>(a.get() * b.get()));
    }

    // template <class T1, class T2, class T3, int S>
    // void
    // mul(FixedPoint<T1, S>& dst, const FixedPoint<T2, S>& a, T3 b)
    // {
    //     dst.set(a.get() * b);
    // }

    inline void
    div(float &dst, float a, float b)
    {
        dst = a / b;
    }

    template <class T1, class T2, class T3, int S>
    void
    div(T1 &dst, const FixedPoint<T2, S> &a, const FixedPoint<T3, S> &b)
    {
        dst = a.get() / b.get();
    }

    inline void
    madd(float &dst, float c, float a, float b)
    {
        dst = c + a * b;
    }

    template <int SD, class T, class T1, class T2, int S1, int S2>
    void
    madd(FixedPoint<T, SD> &dst,
         const FixedPoint<T, S1 + S2> &c,
         const FixedPoint<T1, S1> &a,
         const FixedPoint<T2, S2> &b)
    {
        dst.set(detail::shift<SD - S1 - S2>(c.get() + a.get() * b.get()));
    }

    // template <class T, class T1, class T2, class T3, int S>
    // void
    // madd(FixedPoint<T, S>& dst,
    //      const FixedPoint<T1, S>& c,
    //      const FixedPoint<T2, S>& a,
    //      T3 b)
    // {
    //     dst.set(c.get() + a.get() * b);
    // }

    inline void
    nmsub(float &dst, float c, float a, float b)
    {
        dst = c - a * b;
    }

    template <int SD, class T, class T1, class T2, int S1, int S2>
    void
    nmsub(FixedPoint<T, SD> &dst,
          const FixedPoint<T, S1 + S2> &c,
          const FixedPoint<T1, S1> &a,
          const FixedPoint<T2, S2> &b)
    {
        dst.set(detail::shift<SD - S1 - S2>(c.get() - a.get() * b.get()));
    }

    // template <class T, class T1, class T2, class T3, int S>
    // void
    // nmsub(FixedPoint<T, S>& dst,
    //       const FixedPoint<T1, S>& c,
    //       const FixedPoint<T2, S>& a,
    //       T3 b)
    // {
    //     dst.set(c.get() - a.get() * b);
    // }

    inline int clz(uint32_t x)
    {
        static const int __not_in_flash_func(table)[32] = {31, 22, 30, 21, 18, 10, 29, 2, 20, 17, 15, 13, 9, 6, 28, 1,
                                                           23, 19, 11, 3, 16, 14, 7, 24, 12, 4, 8, 25, 5, 26, 27, 0};

        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        return table[(uint32_t)(x * 0x07c4acdd) >> 27];
    }

    template <class T, int S1, int S2>
    void
    log2estimate(FixedPoint<T, S1> &dst, const FixedPoint<T, S2> &v)
    {
        // log2(x) = log2(a * 2^N) = N + log2(a)
        // log2(0b1.xxxx) ~ 0b0.xxxx
        auto uiv = static_cast<uint32_t>(v.get());
        // int lz = __builtin_clz(uiv);
        int lz = clz(uiv);

        uiv <<= (lz + 1);
        uiv >>= 32 - S1;

        T n = 31 - lz - S2;

        dst.set((n << S1) | static_cast<T>(uiv));
    }

    template <class T, int S1, int S2>
    void
    log2estimate2(FixedPoint<T, S1> &dst, const FixedPoint<T, S2> &v)
    {
        // log2(x) = log2(a * 2^N) = N + log2(a)
        // log2(x+1) ~ 4/3 x - 1/3 x^2
        auto uiv = static_cast<uint32_t>(v.get());
        // int lz = __builtin_clz(uiv);
        int lz = clz(uiv);

        static constexpr uint32_t _1_3 = (1.0f / 3) * (1 << 18) + 0.5f;

        uiv <<= (lz + 1);
        uiv >>= 32 - 16;                 // .16 fixed
        uint32_t v2 = (uiv * uiv) >> 18; // .14 fixed
        uint32_t vv = uiv - v2;
        vv *= _1_3; // .32 fixed
        vv = detail::shift<S1 - 32>(vv);

        T n = 31 - lz - S2;
        dst.set((n << S1) + static_cast<T>(vv));
    }

    template <class T, int S1, int S2>
    void
    exp2estimate(FixedPoint<T, S1> &dst, const FixedPoint<T, S2> &v)
    {
        // exp2(x) = exp2(N + a) = exp2(N)exp2(a)
        // exp2(a) ~ a + 1
        int n = detail::shift<-S2>(v.get());
        if (S2 < 0)
        {
            dst.set(detail::dshift(T(1), n + S1));
            return;
        }
        T a = v.get() & (detail::shift<S2>(1) - 1);
        T e2a = a + detail::shift<S2>(1);

        dst.set(detail::dshift(e2a, n + (S1 - S2)));
    }

    template <class T, int S1, int S2>
    void
    exp2estimate2(FixedPoint<T, S1> &dst, const FixedPoint<T, S2> &v)
    {
        static_assert(S2 > 0, "");
        static_assert(S2 <= 16, "");

        // exp2(x) = exp2(N + a) = exp2(N)exp2(a)
        // exp2(a) ~ 1 + 0.653426 x + 0.346574 x^2
        int n = detail::shift<-S2>(v.get());

        static constexpr uint32_t c1 = 0.653426f * (1 << (31 - S2)) + 0.5f;
        static constexpr uint32_t c2 = 0.346574f * (1 << (31 - S2)) + 0.5f;

        uint32_t a = v.get() & (detail::shift<S2>(1) - 1);
        uint32_t a2 = c2 * ((a * a) >> S2);
        uint32_t a1 = c1 * a;
        uint32_t e2a = a1 + a2 + detail::shift<31>(1u); // .31 fixed
        int rshift = 31 - S1 - n;
        dst.set(rshift >= 32 ? 0 : static_cast<T>(e2a >> rshift));
    }

    template <class T, int S1, int S2, int S3>
    void
    powEstimate(FixedPoint<T, S1> &dst,
                const FixedPoint<T, S2> &a,
                const FixedPoint<T, S3> &b)
    {
        FixedPoint<T, 16> t1;
        log2estimate2(t1, a);

        FixedPoint<T, 16> t2;
        mul(t2, t1, b);

        exp2estimate2(dst, t2);
    }

    inline void
    log2estimate2(float &dst, float v)
    {
        dst = log2(v);
    }

    inline void
    exp2estimate2(float &dst, float v)
    {
        dst = exp2(v);
    }

    inline uint32_t
    getAbsMask(float v)
    {
        return 0;
        //    return *(uint32_t*)(&v) & 0x7fffffff;
    }

    template <class T, int S>
    uint32_t
    getAbsMask(const FixedPoint<T, S> &v)
    {
        auto r = v.get();
        return r < 0 ? -r : r;
    }

} // namespace physical_modeling_piano

#endif /* _4CAD8B76_C134_14C4_1746_4D5010DEF3DD */
