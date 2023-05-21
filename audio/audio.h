/*
 * author : Shuichi TAKANO
 * since  : Mon May 01 2023 02:02:53
 */

#include <cstdint>
#include <cstdlib>
#include <array>
#include <functional>
#include <hardware/pio.h>

namespace audio
{
    inline constexpr size_t AUDIO_CHANNELS = 1;

    using SampleFillFunc = std::function<void(std::array<int16_t *, AUDIO_CHANNELS> &buffers,
                                              size_t nSamples)>;

    void initializeAudio(std::initializer_list<int> pins, pio_hw_t *pio);
    void startAudioStream(SampleFillFunc &&f);
}
