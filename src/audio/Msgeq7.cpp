#include "audio/Msgeq7.h"

#include <Arduino.h>

namespace {

// MSGEQ7 datasheet timings, microseconds, each the published minimum rounded up.
// Overshooting only makes a read slower; undershooting reads the previous band.
constexpr unsigned RESET_PULSE_US = 1;       // reset pulse width (100 ns min)
constexpr unsigned RESET_TO_STROBE_US = 72;  // reset released -> first strobe
constexpr unsigned OUTPUT_SETTLE_US = 36;    // strobe low -> output valid
constexpr unsigned STROBE_WIDTH_US = 18;     // strobe high, before the next band

}  // namespace

Msgeq7::Msgeq7(int strobePin, int resetPin, int analogPin)
    : _strobePin(strobePin), _resetPin(resetPin), _analogPin(analogPin) {}

void Msgeq7::Begin() const {
    pinMode(_strobePin, OUTPUT);
    pinMode(_resetPin, OUTPUT);
    // Idle high: a strobe is an active-low pulse, so parking it low would leave
    // the chip mid-band and make the first read of every sequence the wrong one.
    digitalWrite(_strobePin, HIGH);
    digitalWrite(_resetPin, LOW);
}

void Msgeq7::Read(int (&bands)[AUDIO_BAND_COUNT]) const {
    // Rewind the multiplexer to the lowest band. Done on every read rather than
    // once at startup: a missed strobe edge would otherwise skew every band by
    // one for the rest of the session, silently.
    digitalWrite(_resetPin, HIGH);
    delayMicroseconds(RESET_PULSE_US);
    digitalWrite(_resetPin, LOW);
    delayMicroseconds(RESET_TO_STROBE_US);

    for (int band = 0; band < AUDIO_BAND_COUNT; ++band) {
        digitalWrite(_strobePin, LOW);
        delayMicroseconds(OUTPUT_SETTLE_US);   // the value is invalid before this
        bands[band] = analogRead(_analogPin);
        digitalWrite(_strobePin, HIGH);
        delayMicroseconds(STROBE_WIDTH_US);
    }
}
