/*
 * author : Shuichi TAKANO
 * since  : Sat Apr 20 2019 3:51:56
 */

#include "filter.h"
#include <math.h>
#include <stdio.h>

namespace physical_modeling_piano
{

namespace
{
constexpr float PI = 3.1415927f;
} // namespace

namespace detail
{
void
dumpFilter(const char* str, int n, const float* a, const float* b)
{
#if 0
    printf("Filter: %s\n", str);

    printf("a: ");
    for (int i = 0; i < n + 1; ++i)
        printf("%g ", a[i]);
    printf("\n");

    printf("b: ");
    for (int i = 0; i < n + 1; ++i)
        printf("%g ", b[i]);
    printf("\n\n");

#if 0
    static constexpr int PS = 1 << 11;
    printf("af: ");
    for (int i = 0; i < n + 1; ++i)
        printf("%d ", (int32_t)(a[i] * PS));
    printf("\n");

    printf("bf: ");
    for (int i = 0; i < n + 1; ++i)
        printf("%d ", (int32_t)(b[i] * PS));
    printf("\n\n");
#endif
#endif
}

void
makeBiquadFilter(
    float* ca, float* cb, float f0, float fs, float Q, BiquadFilterType type)
{
    float a   = 1 / (2 * tanf(PI * f0 / fs));
    float a2  = a * a;
    float aoQ = a / Q;
    float d   = (4 * a2 + 2 * aoQ + 1);

    ca[0] = 1;
    ca[1] = -(8 * a2 - 2) / d;
    ca[2] = (4 * a2 - 2 * aoQ + 1) / d;

    switch (type)
    {
    case BiquadFilterType::ALLPASS:
        cb[0] = 2 * aoQ / d;
        cb[1] = 0;
        cb[2] = -2 * aoQ / d;
        break;

    case BiquadFilterType::LOWPASS:
        cb[0] = 1 / d;
        cb[1] = 2 / d;
        cb[2] = 1 / d;
        break;

    case BiquadFilterType::HIGHPASS:
        cb[0] = 4 * a2 / d;
        cb[1] = -8 * a2 / d;
        cb[2] = 4 * a2 / d;
        break;

    case BiquadFilterType::NOTCH:
        cb[0] = (1 + 4 * a2) / d;
        cb[1] = (2 - 8 * a2) / d;
        cb[2] = (1 + 4 * a2) / d;
        break;
    }
}

void
makeLossFilter(float ca[2], float cb[2], float f0, float fs, float c1, float c3)
{
    float g  = 1 - c1 / f0;
    float b  = 4 * c3 + f0;
    float a1 = (-b + sqrtf(b * b - 16 * c3 * c3)) / (4 * c3);
    cb[0]    = g * (1 + a1);
    cb[1]    = 0;
    ca[0]    = 1;
    ca[1]    = a1;

    dumpFilter("loss", 1, ca, cb);
}

namespace
{

float
Db(float B, float f, int M)
{
    float C1, C2, k1, k2, k3;
    if (M == 4)
    {
        C1 = 0.069618f;
        C2 = 2.0427f;
        k1 = -0.00050469f;
        k2 = -0.0064264f;
        k3 = -2.8743f;
    }
    else
    {
        C1 = 0.071089f;
        C2 = 2.1074f;
        k1 = -0.0026580f;
        k2 = -0.014811f;
        k3 = -2.9018f;
    }

    float logB     = logf(B);
    float kd       = expf(k1 * logB * logB + k2 * logB + k3);
    float Cd       = expf(C1 * logB + C2);
    float halfstep = powf(2.0f, 1 / 12.0f);
    float Ikey     = logf(f * halfstep / 27.5f) / log(halfstep);
    float D        = expf(Cd - Ikey * kd);

    return D;
}

using Complex = std::array<float, 2>;

Complex
div(const Complex& Hn, const Complex& Hd)
{
    float magn = sqrtf(Hn[0] * Hn[0] + Hn[1] * Hn[1]);
    float argn = atan2f(Hn[1], Hn[0]);
    float magd = sqrtf(Hd[0] * Hd[0] + Hd[1] * Hd[1]);
    float argd = atan2f(Hd[1], Hd[0]);
    float mag  = magn / magd;
    float arg  = argn - argd;

    return {mag * cosf(arg), mag * sinf(arg)};
}

} // namespace

float
computePhaseDelay(int cn, const float* ca, const float* cb, float f, float Fs)
{
    Complex Hn{};
    Complex Hd{};

    float omega = 2 * PI * f / Fs;
    for (int i = 0; i <= cn; ++i)
    {
        Hn[0] += cosf(i * omega) * cb[i];
        Hn[1] += sinf(i * omega) * cb[i];
    }
    for (int i = 0; i <= cn; ++i)
    {
        Hd[0] += cosf(i * omega) * ca[i];
        Hd[1] += sinf(i * omega) * ca[i];
    }

    auto H    = div(Hn, Hd);
    float arg = atan2f(H[1], H[0]);
    if (arg < 0)
    {
        arg = arg + 2 * PI;
    }

    return arg / omega;
}

float
computeGroupDelay(int cn, const float* ca, const float* cb, float f, float Fs)
{
    float df     = 5;
    float f2     = f + df;
    float f1     = f - df;
    float omega2 = 2 * PI * f2 / Fs;
    float omega1 = 2 * PI * f1 / Fs;
    return (omega2 * computePhaseDelay(cn, ca, cb, f2, Fs) -
            omega1 * computePhaseDelay(cn, ca, cb, f1, Fs)) /
           (omega2 - omega1);
}

void
thirian(int cn, float* ca, float* cb, float D)
{
    if (D <= 1.0f)
    {
        ca[0]  = 1;
        cb[cn] = 1;
        for (int i = 1; i <= cn; ++i)
        {
            ca[i]      = 0;
            cb[cn - i] = 0;
        }
        return;
    }
    //    printf("D=%f %d\n", D, cn);
    for (int i = 0; i <= cn; ++i)
    {
        auto choose = [&] {
            int divisor    = 1;
            int multiplier = cn;
            int answer     = 1;
            auto k         = std::min(i, cn - i);
            while (divisor <= k)
            {
                answer = answer * multiplier / divisor;
                --multiplier;
                ++divisor;
            }
            return answer;
        };

        float ai = choose();
        //        printf("ai = %f\n", ai);
        if (i & 1)
        {
            ai = -ai;
        }
        for (int n = 0; n <= cn; ++n)
        {
            ai *= (D - (cn - n)) / (D - (cn - n - i));
            //            printf(" %d: %f\n", n, ai);
        }
        ca[i]      = ai;
        cb[cn - i] = ai;
    }
}

void
makeThirianDispersionFilter(float* ca, float* cb, float B, float f, int M)
{
    float D = Db(B, f, M);
    if (D <= 1.0f)
    {
        ca[0] = 1;
        ca[1] = 0;
        ca[2] = 0;
        cb[0] = 1;
        cb[1] = 0;
        cb[2] = 0;
    }
    else
    {
        thirian(2, ca, cb, D);
    }

    dumpFilter("thirian dispersion", 2, ca, cb);
}

} // namespace detail

} // namespace physical_modeling_piano
