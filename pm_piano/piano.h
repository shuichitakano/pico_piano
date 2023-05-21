/*
 * author : Shuichi TAKANO
 * since  : Sun May 12 2019 23:12:11
 */
#ifndef DC39274B_A134_1524_1625_4EACE18FA496
#define DC39274B_A134_1524_1625_4EACE18FA496

#include "note_manager.h"
#include "soundboard.h"
#include <midi.h>

#include <pico/platform.h>

namespace physical_modeling_piano
{
    class Piano
    {
        NoteManager noteManager_;
        Soundboard soundboard_;

        SystemParameters sysParams_;
        PedalState pedal_;

    public:
        Piano() {}

        void initialize(size_t nPoly);

        void __time_critical_func(update)(int16_t *dst, size_t nSamples,
                                          io::MidiMessageQueue &midiIn);

        size_t getCurrentNoteCount() const
        {
            return noteManager_.getCurrentNoteCount();
        }
        const std::array<bool, 88> &getKeyOnStateForDisp() const
        {
            return noteManager_.getKeyOnStateForDisp();
        }

        void worker() { noteManager_.worker(); }
    };

} // namespace physical_modeling_piano

#endif /* DC39274B_A134_1524_1625_4EACE18FA496 */
