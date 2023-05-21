/*
 * author : Shuichi TAKANO
 * since  : Wed May 01 2019 21:20:45
 */

#include "hammer.h"
#include <math.h>

namespace physical_modeling_piano
{

void
Hammer::initialize(float m,
                   float K,
                   float p,
                   float Z,
                   float alpha,
                   const SystemParameters& sysParams)
{
    p_  = p;
    c1_ = log2(K / (2 * Z));
    c2_ = alpha / sysParams.deltaT;
    c3_ = sysParams.deltaT * (2 * Z) / m;

    c2h_ = c2_ * 2.0f;
    c3h_ = c3_ * 0.5f;
    // c2h_.set(c2_.get() << 1);
    // c3h_.set(c3_.get() >> 1);
}

void
Hammer::update(State& s,
               const VelocityT& vin,
               const SystemParameters& sysParams) const
{
    //      printf("v:%g vin:%g u:%g\n", (float)s.v, (float)vin,
    //      (float)s.u);
    // u += (v - vin - F_2Z) * dt;
    VelocityT tv;
    sub(tv, s.v, vin);
    sub(tv, tv, s.F_2Z);
    FeltCompT du;
    mul(du, tv, sysParams.deltaTF);
    // mul(du, tv, sysParams.deltaT);
    add(s.u, s.u, du);

    // upK_2Z = u > 0 ? pow(u, p) * (K/2Z) : 0
    LogSpaceT tl;
    log2estimate2(tl, s.u);
    madd(tl, c1_, tl, p_);

    FeltCompPT upK_2Z;
    exp2estimate2(upK_2Z, tl);
    if (!isPlus(s.u))
    {
        upK_2Z = 0;
    }

    //        printf("upK_2Z: %g F_2Z: %g\n", (float)upK_2Z, (float)s.F_2Z);

    // dupK_2Z = upK_2Z - prev_upK_2Z
    FeltCompPT dupK_2Z;
    sub(dupK_2Z, upK_2Z, s.prev_upK_2Z);

    // F_2Z = max(0, upK_2Z + (alpha/dt) * dupK_2Z)
    ResultT tf;
    mul(tf, c2_, dupK_2Z);
    add(s.F_2Z, upK_2Z, tf);
    clamp0(s.F_2Z, s.F_2Z);

    // v -= F_2Z * (dt*2Z/m)
    VelocityT dv;
    mul(dv, s.F_2Z, c3_);
    sub(s.v, s.v, dv);

    s.prev_upK_2Z = upK_2Z;
}

#if 0
// fast/low prec
#define log2est log2estimate
#define exp2est exp2estimate
#else
#define log2est log2estimate2
#define exp2est exp2estimate2
#endif

void
Hammer::computeVelocity(VelocityT& dstV,
                        FeltCompT& dstU,
                        ResultT& dstF_2Z,
                        FeltCompPT& upK_2Z,
                        const VelocityT& v,
                        const FeltCompT& u,
                        const ResultT& F_2Z,
                        const VelocityT& vin,
                        const DeltaTimeT& dt,
                        const FeltCompPT& prev_upK_2Z,
                        const C2T& c2,
                        const C3T& c3) const
{
    // u = u + (v - (vin + F_2Z)) * dt;
    VelocityT tv;
    sub(tv, v, vin);
    sub(tv, tv, F_2Z);
    FeltCompT du;
    mul(du, tv, dt);
    add(dstU, u, du);

    // upK_2Z = u > 0 ? pow(u, p) * (K/2Z) : 0
    LogSpaceT tl;
    log2est(tl, dstU);
    madd(tl, c1_, tl, p_);

    exp2est(upK_2Z, tl);
    if (!isPlus(dstU))
    {
        upK_2Z = 0;
    }

    // dupK_2Z = upK_2Z - prev_upK_2Z
    FeltCompPT dupK_2Z;
    sub(dupK_2Z, upK_2Z, prev_upK_2Z);

    // F_2Z = max(0, upK_2Z + (alpha/dt) * dupK_2Z)
    ResultT tf;
    mul(tf, c2, dupK_2Z);
    add(dstF_2Z, upK_2Z, tf);
    clamp0(dstF_2Z, dstF_2Z);

    // v = v - F_2Z * (dt*2Z/m)
    VelocityT dv;
    mul(dv, dstF_2Z, c3);
    sub(dstV, v, dv);
}

void
Hammer::update2(State& s,
                const VelocityT& vin,
                const SystemParameters& sysParams) const
{
#if 1
    VelocityT vc;
    FeltCompT uc;
    ResultT F_2Zc;
    FeltCompPT upK_2Z;

    computeVelocity(vc,
                    uc,
                    F_2Zc,
                    upK_2Z,
                    s.v,
                    s.u,
                    s.F_2Z,
                    vin,
                    sysParams.deltaT_2F,
                    s.prev_upK_2Z,
                    c2h_,
                    c3h_);

    computeVelocity(s.v,
                    s.u,
                    s.F_2Z,
                    upK_2Z,
                    vc,
                    s.u,
                    F_2Zc,
                    vin,
                    sysParams.deltaTF,
                    s.prev_upK_2Z,
                    c2_,
                    c3_);

    s.prev_upK_2Z = upK_2Z;

#else
    // uc = u + (v - vin - F_2Z) * (dt*0.5);
    FeltCompT uc;
    VelocityT tv;
    sub(tv, s.v, vin);
    sub(tv, tv, s.F_2Z);
    FeltCompT du;
    mul(du, tv, sysParams.deltaT_2F);
    add(uc, s.u, du);

    // upK_2Z = uc > 0 ? pow(uc, p) * (K/2Z) : 0
    LogSpaceT tl;
    log2est(tl, uc);
    madd(tl, c1_, tl, p_);

    FeltCompPT upK_2Zh;
    exp2est(upK_2Zh, tl);
    if (!isPlus(uc))
    {
        upK_2Zh = 0;
    }

    // dupK_2Z = upK_2Z - prev_upK_2Z
    FeltCompPT dupK_2Z;
    sub(dupK_2Z, upK_2Zh, s.prev_upK_2Z);

    // F_2Z = max(0, upK_2Z + (alpha/(dt*0.5)) * dupK_2Z)
    ResultT F_2Zh;
    ResultT tf;
    mul(tf, c2h_, dupK_2Z);
    add(F_2Zh, upK_2Zh, tf);
    clamp0(F_2Zh, F_2Zh);

    // vc = v - F_2Z * (0.5*dt*2Z/m)
    VelocityT vc;
    VelocityT dv;
    mul(dv, F_2Zh, c3h_);
    sub(vc, s.v, dv);

    // u += (vc - vin - F_2Z) * dt;
    sub(tv, vc, vin);
    sub(tv, tv, F_2Zh);
    mul(du, tv, sysParams.deltaTF);
    add(s.u, s.u, du);

    // upK_2Z = u > 0 ? pow(u, p) * (K/2Z) : 0
    log2est(tl, s.u);
    madd(tl, c1_, tl, p_);

    FeltCompPT upK_2Z;
    exp2est(upK_2Z, tl);
    if (!isPlus(s.u))
    {
        upK_2Z = 0;
    }

    // dupK_2Z = upK_2Z - prev_upK_2Z
    sub(dupK_2Z, upK_2Z, s.prev_upK_2Z);

    // F_2Z = max(0, upK_2Z + (alpha/dt) * dupK_2Z)
    mul(tf, c2_, dupK_2Z);
    add(s.F_2Z, upK_2Z, tf);
    clamp0(s.F_2Z, s.F_2Z);

    // v -= F_2Z * (dt*2Z/m)
    mul(dv, s.F_2Z, c3_);
    sub(s.v, s.v, dv);

    s.prev_upK_2Z = upK_2Z;
#endif
}

void
Hammer::update4(State& s,
                const VelocityT& vin,
                const SystemParameters& sysParams) const
{
    ResultT F_2Zc;
    FeltCompPT upK_2Z;

    VelocityT v2;
    FeltCompT u2;

    computeVelocity(v2,
                    u2,
                    F_2Zc,
                    upK_2Z,
                    s.v,
                    s.u,
                    s.F_2Z,
                    vin,
                    sysParams.deltaT_2F,
                    s.prev_upK_2Z,
                    c2h_,
                    c3h_);

    VelocityT v3;
    FeltCompT u3;

    computeVelocity(v3,
                    u3,
                    F_2Zc,
                    upK_2Z,
                    v2,
                    u2,
                    s.F_2Z,
                    vin,
                    sysParams.deltaT_2F,
                    s.prev_upK_2Z,
                    c2h_,
                    c3h_);

    VelocityT v4;
    FeltCompT u4;

    computeVelocity(v4,
                    u4,
                    F_2Zc,
                    upK_2Z,
                    v3,
                    s.u,
                    s.F_2Z,
                    vin,
                    sysParams.deltaTF,
                    s.prev_upK_2Z,
                    c2_,
                    c3_);

    // v = (v1 + 2 * v2 + 2 * v3 + v4) / 6
    VelocityT v;
    add(v, v2, v3);
    shift<1>(v, v);
    add(v, v, s.v);
    add(v, v, v4);

    static constexpr FixedPoint<int32_t, 8> _1_6{1.0f / 6.0f};
    mul(v, v, _1_6);

    computeVelocity(s.v,
                    s.u,
                    s.F_2Z,
                    upK_2Z,
                    v,
                    s.u,
                    F_2Zc,
                    vin,
                    sysParams.deltaTF,
                    s.prev_upK_2Z,
                    c2_,
                    c3_);

    s.prev_upK_2Z = upK_2Z;
}

} // namespace physical_modeling_piano