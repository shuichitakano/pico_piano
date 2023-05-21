#include <stdio.h>
#include <cstdint>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/interp.h"
#include "hardware/gpio.h"
#include "hardware/vreg.h"
#include <pico/multicore.h>
#include "pico/cyw43_arch.h"
#include "btstack.h"
#include "bt.h"
#include "hci.h"
#include <string>
#include <optional>

#include <pm_piano/piano.h>
#include <audio/audio.h>
#include <math.h>

// #include <btstack_tlv.h>

#include "ble_client_manager.h"
#include "ble_midi.h"

physical_modeling_piano::Piano piano_;
io::MidiMessageQueue midiIn_;

int16_t sinTable[1024];

uint32_t *test = 0;

void __not_in_flash_func(core1_main)()
{
    uint32_t guard = 0xdeadbeaf;
    test = &guard;

    for (int i = 0; i < 1024; ++i)
    {
        sinTable[i] = int(sin(i * 2 * 3.14159265f / 1024) * 16384);
    }

    int phase = 0;
    audio::startAudioStream(
        [&](std::array<int16_t *, audio::AUDIO_CHANNELS> & buffers, size_t nSamples) __attribute__((always_inline)) {
#if 0
            while (nSamples)
            {
                int v = sinTable[(phase >> 16) & 1023];
                *buffers[0]++ = v;
                *buffers[1]++ = -v;
                constexpr int step = 440 * 1024 / 48000 * 65536;
                phase += step;
                --nSamples;
            };
#else
            piano_.update(buffers[0], nSamples, midiIn_);
#endif
        });

    while (1)
    {
        tight_loop_contents();
    }
}

int main()
{
#if 1
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(230400, true);
#else
    set_sys_clock_khz(115200, true);
#endif

    gpio_init(6);
    gpio_set_dir(6, GPIO_OUT);
    gpio_put(6, 0);

    // 15750*3*14/15 = 44100
    // 176.4MHzはいいかも
    // 176400000/44100 = 4000
    // 176400000/44100/32=125

    // できなかった…

    // 48KHzならどうにでも
    // 115.2MHz or 230.4MHzとか
    // 230400000/32/150 = 48000
    // 150 = 3*2*5*5

    // 32KHzだと？
    // 230400000/32/225 = 32000
    // 225 = 5*5*3*3
    //

    // 24KHzだと
    // 230400000/32/300 = 24000
    // 300 = 3*2*2*5*5 = 6 * 50 = 12 * 25

    stdio_init_all();

#define PIN_AUDIO_L 2
#define PIN_AUDIO_R 3
    audio::initializeAudio({PIN_AUDIO_L, PIN_AUDIO_R}, pio0);

    if (cyw43_arch_init())
    {
        printf("Wi-Fi init failed");
        return -1;
    }

    bluetooth::initializeBluetooth(true /* client */, false /* server */, IO_CAPABILITY_DISPLAY_YES_NO,
                                   true /* secure */,
                                   true /* bonding */,
                                   true /* MITM */,
                                   false /* Key */);

    att_server_init(profile_data, NULL, NULL);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    auto &bleMidi = io::BLEMidiClient::instance();
    bleMidi.setMIDIIn(&midiIn_);
    bluetooth::BLEClientManager::instance().registerHandler(&bleMidi);

    midiIn_.setActive(true);

    piano_.initialize(9);

    multicore_launch_core1(core1_main);

    piano_.worker();

    while (true)
    {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        //        bleMidi.put({0x80, 0x35, 0x5a});
        sleep_ms(250);
        //        bleMidi.put({0x90, 0x35, 0x5a});
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(250);
    }

    return 0;
}
