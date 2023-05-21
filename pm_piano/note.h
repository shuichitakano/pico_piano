/*
 * author : Shuichi TAKANO
 * since  : Sat Apr 13 2019 21:6:46
 */
#ifndef _2B725238_D134_14C6_1418_0DD49F85A75C
#define _2B725238_D134_14C6_1418_0DD49F85A75C

#include "hammer.h"
#include "pedal.h"
#include "string.h"
#include <vector>
#include <array>

#include <pico/platform.h>

namespace physical_modeling_piano
{

    struct SystemParameters;

    class Note
    {
    public:
        using SampleT = String::SampleT;

        struct State
        {
            String::State strings[3];
            Hammer::State hammer;

            std::vector<uint32_t> allocatorBuffer_;

        public:
            void initialize(size_t allocatorSize);

            bool keyOn{};
            bool sostenuto{};
            bool idle{};
        };

    public:
        void initialize(float freq, const SystemParameters &sysParams);
        size_t computeAllocatorSize() const;

        void __time_critical_func(keyOn)(State &state, Hammer::VelocityT v) const;
        void __time_critical_func(keyOff)(State &state) const;

        void __time_critical_func(update)(SampleT *sample,
                                          uint32_t nSamples,
                                          State &state,
                                          const SystemParameters &sysParams,
                                          const PedalState &pedal) const;

    private:
        int nStrings_{};
        FixedPoint<int32_t, 8> _nStrings_;
        FixedPoint<int32_t, 25> bridgeLoadRatio_;

        String strings_[3];
        Hammer hammer_;
        Hammer::UpdateFunc hammerUpdateFunc_;
    };

} // namespace physical_modeling_piano

#endif /* _2B725238_D134_14C6_1418_0DD49F85A75C */
