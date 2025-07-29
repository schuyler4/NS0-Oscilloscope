# NS0 Oscilloscope
This is firmware to turn your RPi Pico into a very rudimentary, single channel, 500kS/s oscilloscope. For a more capable scope, see the [NS1](https://hackaday.io/project/197104-ns1-oscilloscope).
## I/O
The signal input uses the A0 analog channel on pin 31 with respect to boards ground. With no analog front end, the input voltage can only be
between 0 and 3.3V. Also, when the input is left floating, the voltage that is read by the oscilloscope will not appear to be zero. In addition 
to the input pin, a 3.3V square wave signal is also included on pin 19 for test purposes.
## Analog Front End
TODO
## Control
The oscilloscope system is controlled by a computer that the Pi Pico is connected to. This is done using basic serial over USB. The instrument control
software [voltpeek](https://github.com/schuyler4/voltpeek) provides a UI and Python API to control the oscilloscope. 
## Triggering
Currently, only auto trigger is supported. The waveform is continually captured and displayed without a trigger event.      
## Getting Started
1) Download the NS0.uf2 firmware file from the latest release of this repository.
2) Hold down the bootsel button on your Pico while plugging it in. The Pico should mount as a USB device.
3) Open up the Pico USB device and transfer the firmware file into it.
4) You should now be ready to go. See the [voltpeek readme](https://github.com/schuyler4/voltpeek) to get started with voltpeek and 
capture and display some waveforms.