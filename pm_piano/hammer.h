/*
 * author : Shuichi TAKANO
 * since  : Wed May 01 2019 21:17:19
 */
#ifndef _1691AA6A_4134_1527_1431_04F134D9BCB4
#define _1691AA6A_4134_1527_1431_04F134D9BCB4

#include "fixed.h"
#include "sys_params.h"

#include <pico/platform.h>

namespace physical_modeling_piano
{

    class Hammer
    {
    public:
#if USE_FIXED_POINT
        // velocityはもうちょっと精度が欲しい?
        using ResultT = FixedPoint<int32_t, 13>;   // F... 1000 くらいまで
        using FeltCompT = FixedPoint<int32_t, 22>; // u
        using FeltCompPT = ResultT;                // u^p... ~1000?
        using VelocityT = ResultT;                 // v

        using StiffExpT = FixedPoint<int32_t, 6>; // p [2:3]

        using C1T = FixedPoint<int32_t, 22>; // log2(K/2Z) [22.3628:35.4069]
        using C2T = FixedPoint<int32_t, 8>;  // alpha/dt   [0:0.441]
        using C3T = FixedPoint<int32_t, 13>; // dt*2Z/m    [0.00571992:0.0290235]

        using LogSpaceT = FixedPoint<int32_t, 16>;
#else
        using ResultT = float;
        using FeltCompT = float;
        using FeltCompPT = float;
        using VelocityT = float;
        using StiffExpT = float;
        using C1T = float;
        using C2T = float;
        using C3T = float;
        using LogSpaceT = float;
#endif

        using DeltaTimeT = SystemParameters::DeltaTimeT;

    public:
        struct State
        {
            VelocityT v;
            FeltCompT u;
            FeltCompPT prev_upK_2Z;
            ResultT F_2Z;
            bool idle;

            void reset(VelocityT _v)
            {
                v = _v;
                u = 0;
                prev_upK_2Z = 0;
                F_2Z = 0;
                idle = false;
            }
        };

    public:
        void initialize(float m,
                        float K,
                        float p,
                        float Z,
                        float alpha,
                        const SystemParameters &sysParams);

        void __time_critical_func(update)(State &s,
                                          const VelocityT &vin,
                                          const SystemParameters &sysParams) const;
        void __time_critical_func(update2)(State &s,
                                           const VelocityT &vin,
                                           const SystemParameters &sysParams) const;
        void __time_critical_func(update4)(State &s,
                                           const VelocityT &vin,
                                           const SystemParameters &sysParams) const;

        using UpdateFunc =
            void (Hammer::*)(State &s,
                             const VelocityT &vin,
                             const SystemParameters &sysParams) const;

    protected:
        inline void __time_critical_func(computeVelocity)(VelocityT &dstV,
                                                          FeltCompT &dstU,
                                                          ResultT &dstF_2Z,
                                                          FeltCompPT &upK_2Z,
                                                          const VelocityT &v,
                                                          const FeltCompT &u,
                                                          const ResultT &F_2Z,
                                                          const VelocityT &vin,
                                                          const DeltaTimeT &dt,
                                                          const FeltCompPT &prev_upK_2Z,
                                                          const C2T &c2,
                                                          const C3T &c3) const;

    private:
        StiffExpT p_;
        C1T c1_;
        C2T c2_;
        C3T c3_;

        C2T c2h_;
        C3T c3h_;
    };

} // namespace physical_modeling_piano

#endif /* _1691AA6A_4134_1527_1431_04F134D9BCB4 */
