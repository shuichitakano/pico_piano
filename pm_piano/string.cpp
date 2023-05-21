/*
 * author : Shuichi TAKANO
 * since  : Mon Apr 22 2019 1:56:21
 */

#include "string.h"
#include "sys_params.h"
#include <algorithm>

namespace physical_modeling_piano
{

void
String::initialize(
    float f, float B, float Z, float Zb, const SystemParameters& sysParams)
{
    float Fs         = sysParams.sampleRate;
    float delayTotal = Fs / f;
    auto delay1 =
        std::max(1, (int)(sysParams.hammerPosition * 0.5f * delayTotal));

    M_ = (f > 400) ? 1 : 4;
    for (int i = 0; i < M_; ++i)
    {
        dispersion_[i].initialize(B, f, M_);
    }
    for (int i = M_; i < 4; ++i)
    {
        dispersion_[i].reset();
    }
    float dispersionDelay = M_ * dispersion_[0].computeGroupDelay(f, Fs);

    lowpass_.initialize(f, Fs, sysParams.stringLossC1, sysParams.stringLossC3);
    float lowpassDelay = lowpass_.computeGroupDelay(f, Fs);

    int delay2 =
        std::max(1, (int)(0.5f * (delayTotal - 2 * delay1) - dispersionDelay));
    int delay3 =
        std::max(1, (int)(0.5f * (delayTotal - 2 * delay1) - lowpassDelay - 5));

    // printf("raw d1 = %f d2 = %f d3 = %f\n",
    //        sysParams.hammerPosition * 0.5f * delayTotal,
    //        0.5f * (delayTotal - 2 * delay1) - dispersionDelay,
    //        0.5f * (delayTotal - 2 * delay1) - lowpassDelay - 1);

    auto D = delayTotal -
             (delay1 * 2 + delay2 + delay3 + dispersionDelay + lowpassDelay);
    //    fracDelay_.initialize(D, (int)(D + 0.5f));
    fracDelay_.initialize(D, std::max(1, (int)(D)));
    float tuningDelay = fracDelay_.computeGroupDelay(f, Fs);
    (void)tuningDelay;

#if 0
    printf("total delay=%f/%f left=%d/%d right=%d/%d dispersion=%f lowpass=%f "
           "frac=%f/%f\n",
           delay1 * 2 + delay2 + delay3 + dispersionDelay + lowpassDelay +
               tuningDelay,
           delayTotal,
           delay1,
           delay1,
           delay2,
           delay3,
           dispersionDelay,
           lowpassDelay,
           tuningDelay,
           D);
#endif

    d0a_.initialize(delay1);
    d0b_.initialize(delay1);
    d1a_.initialize(delay2);
    d1b_.initialize(delay3);

    float alpha12 = 2 * Z / (Z + Zb);
    alpha12_      = alpha12;
    // printf("Z:%f Zb:%f alpha12:%f, %f, %d\n",
    //        Z,
    //        Zb,
    //        alpha12,
    //        (float)alpha12_,
    //        alpha12_.get());
}

String::State::State() {}

size_t
String::getStateSize() const
{
    return d0a_.getStateSize() + d0b_.getStateSize() + d1a_.getStateSize() +
           d1b_.getStateSize();
}

} // namespace physical_modeling_piano
