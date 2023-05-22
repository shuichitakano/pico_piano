# pico_piano
This is a physical modelling piano sound engine that runs on Raspberry Pi Pico W.

It is a re-implementation based on C. Otey's physical modelling piano implementation (http://large.stanford.edu/courses/2007/ph210/otey2/).


## How to use
You can play from a BLE MIDI device.
It will connect to the first found BLE MIDI device in its vicinity.

The sound is outputted to GPIO2.
