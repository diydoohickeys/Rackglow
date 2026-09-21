#pragma once

#include "audio/AudioFrame.h"

/**
 * @brief Reads the seven band levels out of an MSGEQ7 graphic equaliser chip.
 *
 * The part exposes ONE analogue output and multiplexes its bands across it:
 * pulsing RESET returns the multiplexer to the lowest band, and each STROBE
 * pulse advances to the next. A read is therefore a fixed sequence of edges and
 * waits, and it blocks for roughly half a millisecond — which is why it lives on
 * the audio task rather than anywhere near the render or network paths.
 *
 * Implemented from the MSGEQ7 datasheet's timing table; the delays are its
 * published minimums, not values anyone tuned by ear.
 */
class Msgeq7 {
public:
    Msgeq7(int strobePin, int resetPin, int analogPin);

    /// Drives the control pins to their idle state. Call once before reading.
    void Begin() const;

    /**
     * @brief Samples all seven bands, lowest first (63 Hz to 16 kHz).
     *
     * Values are raw ADC counts from the analogue pin, left unscaled and
     * unfiltered — the audio task owns both, so that there is one home for them.
     */
    void Read(int (&bands)[AUDIO_BAND_COUNT]) const;

private:
    const int _strobePin;
    const int _resetPin;
    const int _analogPin;
};
