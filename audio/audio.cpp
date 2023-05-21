/*
 * author : Shuichi TAKANO
 * since  : Mon May 01 2023 02:03:57
 */

#include "audio.h"

#include <array>
#include <stdio.h>
#include <hardware/dma.h>
#include <hardware/interp.h>

#include "simple_serialize.pio.h"

namespace audio
{
    namespace
    {
        struct ChannelDMA
        {
            int dmaCtrlCh_;
            int dmaDataCh_;

            int pioSM_;

            void init(int unitSeqWords,
                      pio_hw_t *pio, int pioProgramOfs, int pin)
            {
                dmaCtrlCh_ = dma_claim_unused_channel(true);
                dmaDataCh_ = dma_claim_unused_channel(true);

                pioSM_ = pio_claim_unused_sm(pio, true);
                io::simpleSerializerInit(pio, pioSM_, pioProgramOfs, pin);

                {
                    auto cfg = dma_channel_get_default_config(dmaCtrlCh_);
                    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
                    channel_config_set_read_increment(&cfg, true);
                    channel_config_set_write_increment(&cfg, true);
                    channel_config_set_ring(&cfg, true /* w */, 2 /* bits */);

                    dma_channel_configure(
                        dmaCtrlCh_,
                        &cfg,
                        &dma_hw->ch[dmaDataCh_].al3_read_addr_trig /* write addr */,
                        nullptr /*read addr */,
                        1 /*count*/,
                        false);
                }
                {
                    auto cfg = dma_channel_get_default_config(dmaDataCh_);
                    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
                    channel_config_set_dreq(&cfg, pio_get_dreq(pio, pioSM_, true /* tx */));
                    channel_config_set_chain_to(&cfg, dmaCtrlCh_);
                    channel_config_set_irq_quiet(&cfg, true);

                    dma_channel_configure(
                        dmaDataCh_,
                        &cfg,
                        &pio->txf[pioSM_] /* write addr */,
                        nullptr /* read addr */,
                        unitSeqWords,
                        false);
                }
            }

            void setList(const uint32_t **list)
            {
                dma_channel_set_read_addr(dmaCtrlCh_, list, false);
            }

            void enableIRQ()
            {
                dma_channel_set_irq0_enabled(dmaDataCh_, true);
            }
        };

        ChannelDMA chDMAs_[AUDIO_CHANNELS];
        uint32_t dmaCtrlChMask_ = 0;
        uint32_t dmaDataChMask_ = 0;

        void initDMA()
        {
            for (auto &chDma : chDMAs_)
            {
                dmaCtrlChMask_ |= 1u << chDma.dmaCtrlCh_;
                dmaDataChMask_ |= 1u << chDma.dmaDataCh_;
            }
        }

        void __not_in_flash_func(startDMA)()
        {
            dma_hw->ints0 = dmaDataChMask_;
            dma_start_channel_mask(dmaCtrlChMask_);
        }

        /////

#if 0
        inline constexpr size_t AUDIO_SAMPLE_RATE = 32000;
        inline constexpr size_t UNIT_SEQUENCE_WORDS = 15;
        inline constexpr size_t UNIT_SEQUENCE_BITS = UNIT_SEQUENCE_WORDS * 32;
        inline constexpr size_t OVERSAMPLING_RATE = 15;
#endif

#if 0
        inline constexpr size_t AUDIO_SAMPLE_RATE = 48000;
        inline constexpr size_t UNIT_SEQUENCE_WORDS = 10;
        inline constexpr size_t UNIT_SEQUENCE_BITS = UNIT_SEQUENCE_WORDS * 32;
        inline constexpr size_t OVERSAMPLING_RATE = 15;
#endif

#if 1
        inline constexpr size_t AUDIO_SAMPLE_RATE = 24000;
        inline constexpr size_t UNIT_SEQUENCE_WORDS = 12;
        inline constexpr size_t UNIT_SEQUENCE_BITS = UNIT_SEQUENCE_WORDS * 32;
        inline constexpr size_t OVERSAMPLING_RATE = 25;
#endif

        inline constexpr size_t CPU_CLOCK = AUDIO_SAMPLE_RATE * UNIT_SEQUENCE_BITS * OVERSAMPLING_RATE;

        // inline constexpr size_t HALF_RING_SAMPLES = 32;
        inline constexpr size_t HALF_RING_SAMPLES = 64;
        inline constexpr size_t HALF_RING_OVERSAMPLING_SAMPLES = HALF_RING_SAMPLES * OVERSAMPLING_RATE;

        using UnitSequence = std::array<uint32_t, UNIT_SEQUENCE_WORDS>;
        UnitSequence unitSequenceTable_[UNIT_SEQUENCE_BITS];

        using HalfRingBuffer = std::array<const UnitSequence *,
                                          HALF_RING_OVERSAMPLING_SAMPLES + 1 /*terminator*/>;
        HalfRingBuffer halfRingBuffer_[2 /* double */][AUDIO_CHANNELS]{};
        int chResidual_[AUDIO_CHANNELS]{};
        int halfRingDBID_ = 0;

        SampleFillFunc sampleFillFunc_;

        void
        initUnitSequenceTable()
        {
            for (int v = 0u; v < UNIT_SEQUENCE_BITS; ++v)
            {
                int r = 0;
                uint32_t bits = 0;
                auto &seq = unitSequenceTable_[v];

                for (int i = 0u; i < UNIT_SEQUENCE_WORDS; ++i)
                {
                    for (int j = 0u; j < 32; ++j)
                    {
                        r += v;
                        bits <<= 1;
                        if (r >= UNIT_SEQUENCE_BITS)
                        {
                            bits |= 1;
                            r -= UNIT_SEQUENCE_BITS;
                        }
                    }
                    seq[i] = bits;
                }
            }
        }

        int __not_in_flash_func(makeDither)(HalfRingBuffer &b,
                                            const std::array<int16_t, HALF_RING_SAMPLES> &samples,
                                            int residual)
        {
#if 0
            auto *dst = b.data();
            for (auto sample : samples)
            {
                int ss = (sample + 32768) * UNIT_SEQUENCE_BITS;
                int qs = ss >> 16;
                const auto *tableBase = &unitSequenceTable_[qs];

                int base0 = ss & 0xffff;
                int accum = residual + base0;
                for (int i = 0; i < OVERSAMPLING_RATE; ++i)
                {
                    int r2 = (accum >> 14) & 4;
                    *dst++ = tableBase + (r2 >> 2);
                    accum += base0;
                }

                residual = accum & 0xffff;
            }
            // 暗黙で最後に null が入ってる
#else
            {
                auto c = interp_default_config();
                interp_config_set_shift(&c, 14);
                interp_config_set_mask(&c, 2, 2);
                interp_config_set_add_raw(&c, true);
                interp_set_config(interp0_hw, 0, &c);
            }
            interp0_hw->accum[0] = 0;
            interp0_hw->base[1] = 0;

            auto *dst = b.data();
            for (auto sample : samples)
            {
                int ss = (sample + 32768) * UNIT_SEQUENCE_BITS;
                int qs = ss >> 16;
                const auto *tableBase = &unitSequenceTable_[qs];
                interp0_hw->base[2] = reinterpret_cast<uintptr_t>(tableBase);

                auto b0 = ss & 0xffff;
                interp0_hw->base[0] = b0;
                interp0_hw->accum[0] = residual + b0;
                for (int i = 0; i < OVERSAMPLING_RATE; ++i)
                {
                    *dst++ = reinterpret_cast<const UnitSequence *>(interp0_hw->pop[2]);
                }

                residual = interp0_hw->accum[0] & 0xffff;
            }
#endif
            return residual;
        }

        void __not_in_flash_func(updateHalfRing)(int dbid)
        {
            if (!sampleFillFunc_)
            {
                return;
            }

            std::array<int16_t, HALF_RING_SAMPLES> samples[AUDIO_CHANNELS];
            {
                std::array<int16_t *, AUDIO_CHANNELS> tmp;
                for (int i = 0; i < AUDIO_CHANNELS; ++i)
                {
                    tmp[i] = samples[i].data();
                }
                sampleFillFunc_(tmp, HALF_RING_SAMPLES);

                for (int i = 0; i < AUDIO_CHANNELS; ++i)
                {
                    auto &buffer = halfRingBuffer_[dbid][i];
                    chResidual_[i] = makeDither(buffer, samples[i], chResidual_[i]);
                }
            }
        }

        void __not_in_flash_func(startAudioDMA)(int dbid)
        {
            for (int i = 0; i < AUDIO_CHANNELS; ++i)
            {
                auto &buffer = halfRingBuffer_[dbid][i];
                chDMAs_[i].setList(reinterpret_cast<const uint32_t **>(buffer.data()));
            }
            startDMA();
        }

        void __not_in_flash_func(irqHandler)()
        {
            gpio_put(6, 1);

            // どっちのDMAも同時に終わっているはずなので同時に再開
            startAudioDMA(halfRingDBID_);

            halfRingDBID_ ^= 1;
            updateHalfRing(halfRingDBID_);

            gpio_put(6, 0);
        }
    }

    void initializeAudio(std::initializer_list<int> pins, pio_hw_t *pio)
    {
        initUnitSequenceTable();

        static auto pioProgramOfs = pio_add_program(pio, &simple_serializer_program);

        int i = 0;
        for (auto pin : pins)
        {
            chDMAs_[i].init(UNIT_SEQUENCE_WORDS, pio, pioProgramOfs, pin);
            ++i;
        }

        initDMA();
    }

    void startAudioStream(SampleFillFunc &&f)
    {
        sampleFillFunc_ = std::move(f);

        updateHalfRing(0);
        updateHalfRing(1);

        chDMAs_[0].enableIRQ();
        irq_set_exclusive_handler(DMA_IRQ_0, irqHandler);
        irq_set_enabled(DMA_IRQ_0, true);

        halfRingDBID_ = 1;
        startAudioDMA(0);
    }
}
