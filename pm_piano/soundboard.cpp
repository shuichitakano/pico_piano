/*
 * author : Shuichi TAKANO
 * since  : Thu May 09 2019 2:10:43
 */

#include "soundboard.h"
#include <assert.h>

namespace physical_modeling_piano
{

    namespace
    {
        constexpr size_t
        getDelayLength(int i)
        {
            constexpr size_t delayLengths[] = {37, 87, 181, 271, 359, 592, 687, 721};
            return convertSampleSize(delayLengths[i]);
        }

    } // namespace

    void
    Soundboard::initialize(const SystemParameters &sysParams)
    {
        a_ = sysParams.soundboardFeedback;

        size_t delaySize = 0;

        for (int i = 0; i < 8; ++i)
        {
            auto delay = getDelayLength(i);
            filters_[i].decay.constant.initialize(sysParams.sampleRate / delay,
                                                  sysParams.sampleRate,
                                                  sysParams.soundboardLossC1,
                                                  sysParams.soundboardLossC3);

            delaySize += computeDelayBufferSize(delay);
        }

        printf("delay size = %zd\n", delaySize);
        delayBuffer_.resize(delaySize);

        size_t ofs = 0;
        for (int i = 0; i < 8; ++i)
        {
            auto delay = getDelayLength(i);
            auto size = computeDelayBufferSize(delay);
            filters_[i].delay.attachBuffer(&delayBuffer_[ofs], size);
            ofs += size;
        }
        assert(ofs == delayBuffer_.size());
    }

    void
    Soundboard::setScale(float s)
    {
        scale_ = s / 8.0f;
    }

    namespace
    {
        inline Soundboard::ValueT __time_critical_func(compute)(Soundboard::ValueT t, Soundboard::ValueT o,
                                                                Soundboard::Filters &filters, size_t delayLength)
        {
            Soundboard::ValueT i;
            add(i, t, o);
            return filters.decay.filter(filters.delay.update(i, delayLength));
        }
    }

    void
    Soundboard::update(ResultT *dst, const ValueT *src, size_t nSamples)
    {
        while (nSamples)
        {
            ValueT t;
            mul(t, ot_, a_);
            add(t, t, *src);

#if 0
            ValueT i[8];
            add(i[0], t, o_[1]);
            add(i[1], t, o_[2]);
            add(i[2], t, o_[3]);
            add(i[3], t, o_[4]);
            add(i[4], t, o_[5]);
            add(i[5], t, o_[6]);
            add(i[6], t, o_[7]);
            add(i[7], t, o_[0]);

            o_[0] = filters_[0].decay.filter(filters_[0].delay.update(i[0], getDelayLength(0)));
            o_[1] = filters_[1].decay.filter(filters_[1].delay.update(i[1], getDelayLength(1)));
            o_[2] = filters_[2].decay.filter(filters_[2].delay.update(i[2], getDelayLength(2)));
            o_[3] = filters_[3].decay.filter(filters_[3].delay.update(i[3], getDelayLength(3)));
            o_[4] = filters_[4].decay.filter(filters_[4].delay.update(i[4], getDelayLength(4)));
            o_[5] = filters_[5].decay.filter(filters_[5].delay.update(i[5], getDelayLength(5)));
            o_[6] = filters_[6].decay.filter(filters_[6].delay.update(i[6], getDelayLength(6)));
            o_[7] = filters_[7].decay.filter(filters_[7].delay.update(i[7], getDelayLength(7)));
#else
            auto po0 = o_[0];

            o_[0] = compute(t, o_[1], filters_[0], getDelayLength(0));
            o_[1] = compute(t, o_[2], filters_[1], getDelayLength(1));
            o_[2] = compute(t, o_[3], filters_[2], getDelayLength(2));
            o_[3] = compute(t, o_[4], filters_[3], getDelayLength(3));
            o_[4] = compute(t, o_[5], filters_[4], getDelayLength(4));
            o_[5] = compute(t, o_[6], filters_[5], getDelayLength(5));
            o_[6] = compute(t, o_[7], filters_[6], getDelayLength(6));
            o_[7] = compute(t, po0, filters_[7], getDelayLength(7));
#endif
            ValueT oo, oe;
            add(oe, o_[0], o_[2]);
            add(oe, oe, o_[4]);
            add(oe, oe, o_[6]);
            add(oo, o_[1], o_[3]);
            add(oo, oo, o_[5]);
            add(oo, oo, o_[7]);

            ValueT r;
            sub(r, oe, oo);
            add(ot_, oe, oo);

            ResultT rs;
            mul(*dst, r, scale_);

            ++dst;
            ++src;
            --nSamples;
        }
    }

} // namespace physical_modeling_piano
