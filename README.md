# PrecisionPWM

PWM with precise frequency for Arduino with ATmega 328P (Arduino Duemilanove/UNO/Pro Mini, etc). Works with any digital pin - doesn't require a PWM-capable pin.

Arduino PWM libraries typically only allow the PWM frequency to be integer. This library allows the PWM frequency to be specified to 4 decimal places from .0001 Hz - 999.9999 Hz.

I hacked it from Udo Klein's Flexible Sweep
https://blog.blinkenlight.net/experiments/measurements/flexible-sweep/

Open a serial terminal at 115200 baud to set the PWM frequency

## Configuration ##

PWM_PIN = pin to output PWM (default = 13) DOES NOT HAVE TO BE A PWM-CAPABLE PIN

duty_cycle = expressed as 1/duty (default = 2 -> 1/2 = 50%)