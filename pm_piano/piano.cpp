/*
 * author : Shuichi TAKANO
 * since  : Sun May 12 2019 23:15:6
 */

#include "piano.h"
#include "hardware/gpio.h"

namespace physical_modeling_piano
{
    void
    Piano::initialize(size_t nPoly)
    {
        noteManager_.initialize(sysParams_, nPoly);
        soundboard_.initialize(sysParams_);
    }

    void
    Piano::update(int16_t *dst, size_t nSamples,
                  io::MidiMessageQueue &midiIn)
    {
        io::MidiMessage m;
        while (midiIn.get(&m))
        // if (midiIn.get(&m))
        {
            auto cmd = m.data[0] & 0xf0;
            if (cmd == 0x80)
            {
                noteManager_.keyOff(m.data[1]);
            }
            else if (cmd == 0x90)
            {
                // float v = m.data[2] * (10 / 127.0f);
                constexpr Hammer::VelocityT scale(10 / 127.0f);
                FixedPoint<int32_t, 0> d(m.data[2]);
                Hammer::VelocityT v;
                mul(v, scale, d);
                noteManager_.keyOn(m.data[1], v);
            }
            else if (cmd == 0xb0)
            {
                switch (m.data[1])
                {
                case 64:
                    pedal_.setDamper(m.data[2] >= 64);
                    break;

                case 66:
                    pedal_.setSostenuto(m.data[2] >= 64);
                    break;
                }
            }
        }

        Note::SampleT samples[nSamples];
        memset(samples, 0, sizeof(Note::SampleT) * nSamples);

        noteManager_.update(samples,
                            nSamples,
                            sysParams_,
                            pedal_);

        // gpio_put(6, 1);
        soundboard_.update(reinterpret_cast<Soundboard::ResultT *>(dst), samples, nSamples);
        // gpio_put(6, 0);
    }

    SystemParameters::DeltaTimeT SystemParameters::deltaTF =
        1.0f / SystemParameters::sampleRate;
    SystemParameters::DeltaTimeT SystemParameters::deltaT_2F =
        0.5f / SystemParameters::sampleRate;

} // namespace physical_modeling_piano
