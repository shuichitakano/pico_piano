/*
 * author : Shuichi TAKANO
 * since  : Mon Apr 15 2019 2:11:3
 */
#ifndef BD597027_3134_14C1_2035_5A0066B56A36
#define BD597027_3134_14C1_2035_5A0066B56A36

#include "allocator.h"
#include "delay.h"
#include "filter.h"
#include "fixed.h"
#include "sys_params.h"
#include <algorithm>
#include <array>

#include <pico/platform.h>

namespace physical_modeling_piano
{

    class String
    {
    public:
#if USE_FIXED_POINT
        using BridgeSampleT = FixedPoint<int32_t, 25>;
        using StringSampleT = FixedPoint<int32_t, 20>;
        using FilterSampleT = FixedPoint<int32_t, 15>;
        using FilterConstT = FixedPoint<int16_t, 12>;
        using FilterHistoryT = FixedPoint<int32_t, 27>; // String * FilterConst
        using ImpedanceRatioT = FixedPoint<int32_t, 14>;
        using HammerLoadT = StringSampleT;
        using SampleT = BridgeSampleT;

        // StringSampleは 2^27 くらいの値
        // ImpedanceRatioは 2^-10 くらいの値
        // 27+(14-10)=31
#else
        using BridgeSampleT = float;
        using StringSampleT = float;
        using FilterSampleT = float;
        using FilterConstT = float;
        using FilterHistoryT = float;
        using ImpedanceRatioT = float;
        using HammerLoadT = float;
        using SampleT = float;
#endif

        using ThirianDispersionFilterT =
            ThirianDispersionFilter<FilterConstT, FilterHistoryT>;

        using LossFilterT = LossFilter<FilterConstT, FilterHistoryT>;

        using ThirianFilterT =
            ThirianFilter<7, FilterConstT, FilterHistoryT, FilterSampleT>;

        class DelayNode
        {
        public:
            struct State
            {
                StringSampleT in{}; // 更新タイミングでdelayに入れてしまえば不要
                StringSampleT out{};
                DelayState<StringSampleT> delay;

            public:
                const StringSampleT &getOut() const { return out; }
                void setIn(const StringSampleT &y) { in = y; }
            };

        public:
            void initialize(int d)
            {
                delay_ = std::max(0, d - 1);
                delayBufferSize_ = computeDelayBufferSize(delay_);
                //            printf("d = %d/%d\n", delay_, delayBufferSize_);
                assert(delay_ <= delayBufferSize_);
            }
            void __time_critical_func(update)(State &s) const { s.out = s.delay.update(s.in, delay_); }

            void reset(State &s, SimpleLinearAllocator &allocator) const
            {
                s.in = 0;
                s.out = 0;
                s.delay.attachBuffer(static_cast<StringSampleT *>(allocator.allocate(
                                         delayBufferSize_ * sizeof(StringSampleT))),
                                     delayBufferSize_);
                s.delay.clear(delay_);
            }

            inline size_t getStateSize() const
            {
                return delayBufferSize_ * sizeof(StringSampleT);
            }

        private:
            uint16_t delay_{};
            uint16_t delayBufferSize_{};
        };

        struct State
        {
            DelayNode::State d0a;
            DelayNode::State d0b;
            DelayNode::State d1a;
            DelayNode::State d1b;

            ThirianDispersionFilterT::State dispersion[4];
            LossFilterT::State lowpass;
            ThirianFilterT::State fracDelay;

            State();
        };

    public:
        void initialize(
            float f, float B, float Z, float Zb, const SystemParameters &sysParams);

        size_t getStateSize() const;

        void reset(State &s, SimpleLinearAllocator &allocator) const
        {
            d0a_.reset(s.d0a, allocator);
            d0b_.reset(s.d0b, allocator);
            d1a_.reset(s.d1a, allocator);
            d1b_.reset(s.d1b, allocator);

            dispersion_[0].clear(s.dispersion[0]);
            lowpass_.clear(s.lowpass);
            fracDelay_.clear(s.fracDelay);
        }

        inline StringSampleT getHammerInputVelocity(const State &s) const
        {
            StringSampleT r;
            add(r, s.d0b.getOut(), s.d1a.getOut());
            return r;
        }

        inline StringSampleT getBridgeInputVelocity(const State &s) const
        {
            return s.d1b.getOut();
        }

        inline void updateDelay(State &s) const
        {
            // update関数と統合したい
            d0a_.update(s.d0a);
            d0b_.update(s.d0b);
            d1a_.update(s.d1a);
            d1b_.update(s.d1b);
        }

        inline SampleT
        __time_critical_func(update)(State &s, BridgeSampleT bridgeLoad, HammerLoadT hammerLoad) const
        {
            StringSampleT loadH;
            add(loadH, s.d0b.getOut(), s.d1a.getOut());
            add(loadH, loadH, hammerLoad);

            BridgeSampleT loadB;
            mul(loadB, alpha12_, s.d1b.getOut());

            BridgeSampleT loadB1d;
            add(loadB1d, loadB, bridgeLoad);
            StringSampleT loadB1 = loadB1d;

            StringSampleT tmp0a;
            sub(tmp0a, loadH, s.d0b.getOut());
            s.d0a.setIn(tmp0a);

            StringSampleT tmp0b;
            neg(tmp0b, s.d0a.getOut());
            s.d0b.setIn(tmp0b);

            StringSampleT tmp1b;
            sub(tmp1b, loadH, s.d1a.getOut());
            s.d1b.setIn(filterH(tmp1b, s));

            StringSampleT tmp1a;
            sub(tmp1a, loadB1, s.d1b.getOut());
            s.d1a.setIn(filterB(tmp1a, s));
#if 0
        printf("bl %f, hl %f, lh %f, lb %f, lb1 %f,  d0b %f, d0a %f, d1a %f, "
               "d1b %f\n",
               (float)bridgeLoad,
               (float)hammerLoad,
               (float)loadH,
               (float)loadB,
               (float)loadB1,
               (float)s.d0b.getOut(),
               (float)s.d0a.getOut(),
               (float)s.d1a.getOut(),
               (float)s.d1b.getOut());
#endif
            return loadB;
        }

    protected:
        FilterSampleT __time_critical_func(filterH)(FilterSampleT y, State &s) const
        {
            y = dispersion_[0].filter(y, s.dispersion[0]);
            y = dispersion_[1].filter(y, s.dispersion[1]);
            y = dispersion_[2].filter(y, s.dispersion[2]);
            y = dispersion_[3].filter(y, s.dispersion[3]);
            return y;
        }

        FilterSampleT __time_critical_func(filterB)(FilterSampleT y, State &s) const
        {
            y = lowpass_.filter(y, s.lowpass);
            y = fracDelay_.filter(y, s.fracDelay);
            return y;
        }

    private:
        DelayNode d0a_;
        DelayNode d0b_;
        DelayNode d1a_;
        DelayNode d1b_;

        ImpedanceRatioT alpha12_;

        //     Z         Z         Zb
        // |<-D0a<-|H|<-D1a<-|B|<-0
        // |->D0b->| |->D1b->| |->out

        int M_ = 0;
        ThirianDispersionFilterT dispersion_[4];
        LossFilterT lowpass_;
        ThirianFilterT fracDelay_;
    };

} // namespace physical_modeling_piano

#endif /* BD597027_3134_14C1_2035_5A0066B56A36 */
