/*
 * author : Shuichi TAKANO
 * since  : Sat Apr 13 2019 21:48:2
 */

#include "note.h"
#include "allocator.h"
#include "sys_params.h"
#include <algorithm>
#include <math.h>
#include <stdio.h>

namespace physical_modeling_piano
{
    void
    Note::initialize(float freq, const SystemParameters &sysParams)
    {
        // MIDI
        //   21: A0:   27.5000Hz
        //   69: A4:  440.0000Hz
        //  108: C8: 4186.0090Hz

        //    constexpr float f0        = 27.5;       // A0
        //    constexpr float f87       = 4186.0090f; // C8
        constexpr float lnf0 = 3.3141860f;      // log(f0);
        constexpr float ilnf87mf0 = 0.1989924f; // 1.0f / (log(f87) - lnf0);
        const float lnFreqRate = logf(freq) - lnf0;
        const float keyRate = lnFreqRate * ilnf87mf0;

        constexpr float PI = 3.1415927f;

        const float rho = sysParams.stringDensity;
        const float L = 0.04f + 1.4f / (1 + expf(-3.4f + 1.4f * lnFreqRate));
        const float r = 0.002f * pow(1 + 0.6f * lnFreqRate, -1.4f);
        const float rhoL = PI * r * r * rho;
        const float T = (2 * L * freq) * (2 * L * freq) * rhoL;

        const float Z = sqrtf(T * rhoL);
        const float Zb = sysParams.bridgeImpedance;

        const float E = sysParams.youngsModulus;
        const float rcore = std::min(r, 0.0006f);
        const float B =
            (PI * PI * PI) * E * (rcore * rcore * rcore * rcore) / (4 * L * L * T);

        if (freq < 47.6f /* < G1 */)
        {
            nStrings_ = 1;
        }
        else if (freq < 84.8f /* < F2 */)
        {
            nStrings_ = 2;
        }
        else
        {
            nStrings_ = 3;
        }

        _nStrings_ = 1.0f / nStrings_;

        for (int i = 0; i < nStrings_; ++i)
        {
            strings_[i].initialize(freq * sysParams.tune[i],
                                   B,
                                   Z,
                                   Zb + (nStrings_ - 1) * Z,
                                   sysParams);
        }

        const float alpha = 0.1e-4f * keyRate;
        const float p = 2.0f + keyRate;
        const float m = 0.06f - 0.058f * powf(keyRate, 0.1f);
        const float K = 40.0f * powf(0.7e-3, -p);
        hammer_.initialize(m, K, p, Z, alpha, sysParams);

        float bridgeLoadRatio = 2 * Z / (Z * nStrings_ + Zb);
        bridgeLoadRatio_ = bridgeLoadRatio;

        //    printf("bridgeLoadRatio:%g %g\n", bridgeLoadRatio,
        //    (float)bridgeLoadRatio_);

        if (keyRate < 0.4f)
        {
            hammerUpdateFunc_ = &Hammer::update;
        }
        else if (keyRate < 0.85f)
        {
            hammerUpdateFunc_ = &Hammer::update2;
        }
        else
        {
            hammerUpdateFunc_ = &Hammer::update4;
        }
    }

    size_t
    Note::computeAllocatorSize() const
    {
        size_t s = 0;
        for (int i = 0; i < nStrings_; ++i)
        {
            s += strings_[i].getStateSize();
        }
        //    printf("size = %zd %zd\n", sizeof(State), s);
        return s;
    }

    void
    Note::State::initialize(size_t allocatorSize)
    {
        allocatorBuffer_.resize((allocatorSize + sizeof(uint32_t) - 1) /
                                sizeof(uint32_t));
    }

    void
    Note::keyOn(State &state, Hammer::VelocityT v) const
    {
        //    printf("keyon %f\n", v);
        SimpleLinearAllocator allocator(state.allocatorBuffer_.data(),
                                        state.allocatorBuffer_.size() *
                                            sizeof(uint32_t));

        for (int i = 0; i < nStrings_; ++i)
        {
            strings_[i].reset(state.strings[i], allocator);
        }
        state.hammer.reset(v);
        state.keyOn = true;
        state.sostenuto = false;
        state.idle = false;
    }

    void
    Note::keyOff(State &state) const
    {
        state.keyOn = false;
    }

    void
    Note::update(SampleT *sample,
                 uint32_t nSamples,
                 State &state,
                 const SystemParameters &sysParams,
                 const PedalState &pedal) const
    {
        if (pedal.sostenutoTrigger)
        {
            state.sostenuto = true;
        }
        state.sostenuto &= pedal.sostenuto;

        bool sustain = state.keyOn | state.sostenuto | pedal.damper;

        if (!sustain)
        {
            // todo: もっとマシにミュートする
            state.idle = true;
            return;
        }

        uint32_t hammerMask = 0;

        while (nSamples)
        {
            String::StringSampleT vString = 0;
            String::StringSampleT load = 0;
            for (int i = 0; i < nStrings_; ++i)
            {
                const auto &s = strings_[i];
                auto &ss = state.strings[i];

                add(vString, vString, s.getHammerInputVelocity(ss));
                s.updateDelay(ss);
                add(load, load, s.getBridgeInputVelocity(ss));
            }

            String::BridgeSampleT bload;
            mul(bload, load, bridgeLoadRatio_);

            Hammer::VelocityT vStringAve;
            FixedPoint<int32_t, 18> vStringTmp = vString;
            mul(vStringAve, vStringTmp, _nStrings_);
            if (!state.hammer.idle)
            {
                //            hammer_.update4(state.hammer, vStringAve, sysParams);
                (hammer_.*hammerUpdateFunc_)(state.hammer, vStringAve, sysParams);
            }

            const auto &hload = state.hammer.F_2Z;
            hammerMask |= getAbsMask(hload);

            for (int i = 0; i < nStrings_; ++i)
            {
                auto s = strings_[i].update(state.strings[i], bload, hload);

                add(*sample, *sample, s);
            }
            ++sample;
            --nSamples;
        }

        if (hammerMask == 0)
        {
            state.hammer.idle = true;
        }
    }

} // namespace physical_modeling_piano
