/*
 * author : Shuichi TAKANO
 * since  : Fri Jan 04 2019 23:36:25
 */

#include "midi.h"
#include <assert.h>
#include <mutex>

namespace io
{

    MidiMessage::MidiMessage(uint8_t d0)
        : size(1), data{{d0}}
    {
    }

    MidiMessage::MidiMessage(uint8_t d0, uint8_t d1)
        : size(2), data{{d0, d1}}
    {
    }

    MidiMessage::MidiMessage(uint8_t d0, uint8_t d1, uint8_t d2)
        : size(3), data{{d0, d1, d2}}
    {
    }

    MidiMessage::MidiMessage(const uint8_t *top, const uint8_t *tail)
    {
        size = tail - top;
        assert(size < 3);
        for (int i = 0; i < (int)size; ++i)
        {
            data[i] = top[i];
        }
    }

    bool
    MidiMessage::isEndOfSysEx() const
    {
        return data[size - 1] == 0xf7;
    }

    void
    MidiMessage::dump() const
    {
        DBOUT(("[ "));
        for (int i = 0; i < size; ++i)
        {
            DBOUT(("%02x ", data[i]));
        }
        DBOUT(("]\n"));
    }

    ////

    bool
    MidiMessageQueue::get(MidiMessage *m)
    {
        std::lock_guard lock(lock_);

        if (queue_.size())
        {
            *m = queue_.front();
            queue_.pop_front();
            return true;
        }
        return false;
    }

    void
    MidiMessageQueue::put(const MidiMessage &m)
    {
        if (active_)
        {
            std::lock_guard lock(lock_);
            queue_.push_back(m);
        }
    }

    void
    MidiMessageQueue::setActive(bool f)
    {
        active_ = f;
    }

} // namespace io