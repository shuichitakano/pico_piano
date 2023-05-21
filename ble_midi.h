/*
 * author : Shuichi TAKANO
 * since  : Mon Dec 10 2018 5:26:20
 */
#pragma once

#include "ble_client_manager.h"
#include "midi.h"
#include <vector>
#include <string>

namespace io
{

    class BLEMidiClient : public bluetooth::BLEClientHandler, public MidiOut
    {
        int handle_ = -1;

        bool readable_ = false;
        bool writable_ = false;
        bool writeNRSupported_ = false;

        MidiMessageQueue *midiIn_ = nullptr;
        MidiMessageMaker midiInMessageMaker_;

        std::vector<uint8_t> tmpBuffer_;

    public:
        BLEMidiClient()
        {
            tmpBuffer_.reserve(32);
        }

        // BLEClientHandler
        bool onUpdateAdvertisingReport(const bluetooth::AdvertisingReport &ad) override;
        bool onEnumServiceCharacteristic(const bluetooth::Service &service,
                                         const bluetooth::Characteristic &chr) override;
        void onNotify(const uint8_t *p, size_t size, int handle) override;

        // MIDIOut
        void put(const MidiMessage &m) override;

        void setMIDIIn(MidiMessageQueue *m) { midiIn_ = m; }

        static BLEMidiClient &instance();
    };

} // namespace io
