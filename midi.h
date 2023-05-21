/*
 * author : Shuichi TAKANO
 * since  : Fri Jan 04 2019 22:56:24
 */
#pragma once

#include "debug.h"
#include <array>
#include <debug.h>
#include <deque>
#include "spinlock.h"

namespace io
{

    struct MidiMessage
    {
        uint8_t size{};
        std::array<uint8_t, 3> data{};

        // System Exclusiv は3byteずつ複数の MidiMessage に分割する
        // 途中に挟まるRealtime系は分離する

    public:
        MidiMessage() {}
        MidiMessage(uint8_t d0);
        MidiMessage(uint8_t d0, uint8_t d1);
        MidiMessage(uint8_t d0, uint8_t d1, uint8_t d2);
        MidiMessage(const uint8_t *top, const uint8_t *tail);

        bool isEndOfSysEx() const;
        bool isValid() const { return size; }
        void dump() const;
    };

    /////
    class MidiIn
    {
    public:
        virtual ~MidiIn() = default;
        virtual bool get(MidiMessage *m) = 0;
    };

    /////
    class MidiOut
    {
    public:
        virtual ~MidiOut() = default;
        virtual void put(const MidiMessage &m) = 0;
    };

    /////
    class MidiMessageQueue : public MidiIn, public MidiOut
    {
        std::deque<MidiMessage> queue_;
        bool active_ = false;
        util::SpinLock lock_;

    public:
        bool get(MidiMessage *m) override;

        void put(const MidiMessage &m);
        void setActive(bool f); // 消費先に接続するときに有効にする
    };

    /////
    class MidiMessageMaker
    {
        bool sysEx_ = false;

        MidiMessage m_;
        int pos_ = 0;

    public:
        void reset()
        {
            sysEx_ = false;
            pos_ = 0;
            m_.data[0] = 0;
        }

        template <class Func>
        void analyze(uint8_t data, const Func &func)
        {
            //        DBOUT(("add %02x, size %d, pos %d\n", data, m_.size, pos_));
            if (data < 0x80)
            {
                // data
                if (sysEx_)
                {
                    m_.data[pos_++] = data;
                    if (pos_ == 3)
                    {
                        m_.size = 3;
                        func(m_);
                        pos_ = 0;
                    }
                }
                else
                {
                    if (pos_ == 0)
                    {
                        // running status
                        if (m_.data[0] >= 0x80 && m_.size >= 2)
                        {
                            pos_ = 1;
                        }
                        else
                        {
                            DBOUT(("invalid running status?\n"));
                            return;
                        }
                    }
                    m_.data[pos_++] = data;
                    if (pos_ == m_.size)
                    {
                        func(m_);
                        pos_ = 0;
                    }
                }
            }
            else if (data < 0xf0)
            {
                // message
                static constexpr uint8_t messageSize[] = {
                    3,
                    3,
                    3,
                    3,
                    2,
                    2,
                    3,
                    0,
                };

                m_.size = messageSize[(data >> 4) - 8];
                m_.data[0] = data;
                pos_ = 0;
            }
            else if (data == 0xf0)
            {
                // exclusive begin
                sysEx_ = true;
                pos_ = 0;
            }
            else if (data == 0xf7)
            {
                // exclusive end
                if (sysEx_)
                {
                    m_.data[pos_++] = 0xf7;
                    m_.size = pos_;
                    func(m_);
                    pos_ = 0;
                }
                sysEx_ = false;
            }
            else if (data >= 0xf8)
            {
                // real time message
                // 対応するなら他のメッセージの途中に来ることを考慮する
            }
            else
            {
                // single byte
                m_.data[0] = data;
                m_.size = 1;
                func(m_);
                pos_ = 0;
            }
        }

        template <class Func>
        void analyze(const uint8_t *begin, const uint8_t *end, const Func &func)
        {
            while (begin != end)
            {
                analyze(*begin, func);
                ++begin;
            }
        }
    };

} // namespace io
