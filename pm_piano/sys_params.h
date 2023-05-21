/*
 * author : Shuichi TAKANO
 * since  : Sat Apr 13 2019 21:10:21
 */
#ifndef _4438B837_7134_14C6_141F_F1B7870F56FE
#define _4438B837_7134_14C6_141F_F1B7870F56FE

#include "fixed.h"
#include <stdint.h>

#define USE_FIXED_POINT 1

namespace physical_modeling_piano
{

    struct SystemParameters
    {
        float youngsModulus = 200e9f;  // [Pa]
        float stringDensity = 7850.0f; // [kg/m^3]
        float bridgeImpedance = 4000.0f;
        float stringLossC1 = 0.25f;
        float stringLossC3 = 5.85f;
        float soundboardLossC1 = 20.0f;
        float soundboardLossC3 = 20.0f;
        float soundboardFeedback = -0.25f;
        float hammerPosition = 1.0f / 7.0f;

        float tune[3] = {1, 1.0003f, 0.9996f};

        //    1/44100 *(2^23) = 190.21786848072563
        //    (2^23)/190 = 44150.56842105263 0.1%
        //     190: 8bit
        using DeltaTimeT = FixedPoint<int32_t, 23>; // 1/44100

        // static constexpr uint32_t sampleRate = 44100;
        // static constexpr uint32_t sampleRate = 32000;
        static constexpr uint32_t sampleRate = 24000;
        // static constexpr uint32_t sampleRate = 22050;

        static constexpr float deltaT = 1.0f / sampleRate;
        static DeltaTimeT deltaTF;
        static DeltaTimeT deltaT_2F;
    };

    constexpr size_t
    convertSampleSize(size_t s)
    {
        return s * SystemParameters::sampleRate / 44100;
    }

} // namespace physical_modeling_piano

#endif /* _4438B837_7134_14C6_141F_F1B7870F56FE */
