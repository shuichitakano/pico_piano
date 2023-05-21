/*
 * author : Shuichi TAKANO
 * since  : Mon Dec 10 2018 5:33:5
 */

#include "ble_midi.h"
#include "debug.h"

#define ENABLE_DEBUG_PRINT 1

#if ENABLE_DEBUG_PRINT
#define DB DBOUT
#else
#define DB(x)
#endif

namespace io
{
    namespace
    {
        constexpr const char *serviceUUID_ = "03B80E5A-EDE8-4B33-A751-6CE34EC4C700";
        constexpr const char *charUUID_ = "7772E5DB-3868-4112-A1A9-F2669D106BF3";

    } // namespace

    BLEMidiClient &
    BLEMidiClient::instance()
    {
        static BLEMidiClient inst;
        return inst;
    }

    bool
    BLEMidiClient::onUpdateAdvertisingReport(const bluetooth::AdvertisingReport &ad)
    {
        if (ad.getUUIDString() == serviceUUID_)
        {
            return true;
        }
        return false;
    }

    bool
    BLEMidiClient::onEnumServiceCharacteristic(const bluetooth::Service &service,
                                               const bluetooth::Characteristic &chr)
    {
        if (service.getUUIDString() == serviceUUID_ && chr.getUUIDString() == charUUID_)
        {
            readable_ = chr.hasReadProp();
            writeNRSupported_ = chr.hasWriteWithoutResponseProp();
            writable_ = chr.hasWriteProp() || writeNRSupported_;

            handle_ = chr.getValueHandle();

            return true;
        }
        return false;
    }

    void
    BLEMidiClient::onNotify(const uint8_t *p, size_t size, int handle)
    {
        if (handle == handle_ && midiIn_)
        {
            DB(("Midi in: handle %d, %zd bytes.\n", handle, size));

            if (size < 3)
            {
                DB((" too short message.\n"));
                return;
            }

            auto process = [this](int timeH,
                                  int timeL,
                                  const uint8_t *top,
                                  const uint8_t *bottom)
            {
                DB(("time %d\n", timeL | (timeH << 7)));
                midiInMessageMaker_.analyze(
                    top, bottom, [this](const MidiMessage &m)
                    {
                    midiIn_->put(m);
                    m.dump(); });
            };

            auto tail = p + size;

            if ((*p & 0x80) == 0)
            {
                DB(("invalid timestamp (H) %02x\n", *p));
                return;
            }
            auto timeH = *p & 0x3f;
            ++p;

            while (p < tail)
            {
                if (p + 2 > tail)
                {
                    DB(("invalid message length.\n"));
                    return;
                }

                if ((p[0] & 0x80) == 0)
                {
                    DB(("invalid timestamp (L) %02x\n", p[0]));
                    return;
                }
                auto timeL = p[0] & 0x7f;
                auto messageTop = p + 1;
                p += 2;

                while (p < tail)
                {
                    if (*p & 0x80)
                    {
                        break;
                    }
                    ++p;
                }
                process(timeH, timeL, messageTop, p);
            }
        }
    }

    void
    BLEMidiClient::put(const MidiMessage &m)
    {
        // MidiOut
        if (handle_ < 0 || !writable_ || !m.isValid())
        {
            return;
        }

        // DB(("out:"));
        // m.dump();

        // 本当はqueueしてまとめた方がいい
        int time = 0;
        tmpBuffer_.clear();
        if (m.isEndOfSysEx() && m.size > 1)
        {
            tmpBuffer_.reserve(2 + m.size - 1 + 1 + 1);
            tmpBuffer_.push_back(0x80 + ((time >> 7) & 0x3f));
            tmpBuffer_.push_back(0x80 + (time & 0x7f));
            tmpBuffer_.insert(tmpBuffer_.end(), m.data.data(), m.data.data() + m.size - 1);
            tmpBuffer_.push_back(0x80 + (time & 0x7f));
            tmpBuffer_.push_back(m.data[m.size - 1]);
        }
        else
        {
            tmpBuffer_.reserve(m.size + 2);
            tmpBuffer_.push_back(0x80 + ((time >> 7) & 0x3f));
            tmpBuffer_.push_back(0x80 + (time & 0x7f));
            tmpBuffer_.insert(tmpBuffer_.end(), m.data.data(), m.data.data() + m.size);
        }

        DB(("write %zd bytes.\n", tmpBuffer_.size()));
        write(handle_, tmpBuffer_.data(), tmpBuffer_.size(), !writeNRSupported_);
    }

} // namespace io