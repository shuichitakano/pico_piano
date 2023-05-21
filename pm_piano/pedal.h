/*
 * author : Shuichi TAKANO
 * since  : Sun May 12 2019 13:7:20
 */
#ifndef _8AE87E1E_1134_1524_C777_9AD218489C23
#define _8AE87E1E_1134_1524_C777_9AD218489C23

namespace physical_modeling_piano
{

struct PedalState
{
    bool damper{};
    bool sostenuto{};
    bool sostenutoTrigger{};
    bool shift{};

public:
    void setShift(bool f) { shift = f; }
    void setDamper(bool f) { damper = f; }
    void setSostenuto(bool f)
    {
        sostenutoTrigger = !sostenuto && f;
        sostenuto        = f;
    }
};

} // namespace physical_modeling_piano

#endif /* _8AE87E1E_1134_1524_C777_9AD218489C23 */
