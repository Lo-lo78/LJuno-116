// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace ljuno
{
// TEST122: The four Studio Room / Concert Hall / Plate / Cathedral algorithms
// ported from LSampler-24's FXReverb + FXEngine. New modes use their own
// reverb network; LJuno's historical Reverb 1 and Reverb 2 are unchanged.
// All delay memory is allocated in prepare(), never in render() / configure().
class SamplerRoomReverb final
{
public:
    struct Settings
    {
        int mode = 1; // 1 Studio Room, 2 Concert Hall, 3 Plate, 4 Cathedral
        float size = 80.0f;            // %
        float decaySeconds = 2.0f;     // LJuno's existing seconds control
        float damping = 10.0f;        // %
        float xoverHz = 360.0f;
        float bassMultiplier = 1.2f;
        float predelayMs = 40.0f;
        float early = 20.0f;          // %
        float tone = 55.0f;           // %
        float width = 100.0f;         // %
        float gainDb = 0.0f;
        float gateThresholdDb = -80.0f; // -80 = off
        float gateDecayMs = 300.0f;
    };

    void prepare (double rate)
    {
        sampleRate = std::max (8000.0, rate);
        const auto lineCapacity = static_cast<std::size_t> (std::ceil (sampleRate * .37)) + 8;
        for (auto& line : lines)
            line.assign (lineCapacity, 0.0f);
        const auto predelayCapacity = static_cast<std::size_t> (std::ceil (sampleRate * .45)) + 8;
        predelayLeft.assign (predelayCapacity, 0.0f);
        predelayRight.assign (predelayCapacity, 0.0f);
        reset();
    }

    void reset() noexcept
    {
        for (auto& line : lines)
            std::fill (line.begin(), line.end(), 0.0f);
        std::fill (predelayLeft.begin(), predelayLeft.end(), 0.0f);
        std::fill (predelayRight.begin(), predelayRight.end(), 0.0f);
        writePosition = predelayWritePosition = 0;
        damped.fill (0.0f);
        readLength.fill (0.0f);
        targetLength.fill (0.0f);
        feedback.fill (0.0f);
        tailToneLow.fill (0.0f);
        configured.fill (-999.0f);
        gateLevel = 0.0f;
        tailFramesRemaining = 0;
    }

    // A cached parameter signature keeps the 8-line FDN's exp/pow work out
    // of the per-sample hot path. This may also be called after step locks.
    void configure (const Settings& s) noexcept
    {
        const std::array<float, 13> signature {
            static_cast<float> (s.mode), s.size, s.decaySeconds, s.damping,
            s.xoverHz, s.bassMultiplier, s.predelayMs, s.early, s.tone,
            s.width, s.gainDb, s.gateThresholdDb, s.gateDecayMs
        };
        if (signature == configured)
            return;
        if (configured[0] > 0.0f && configured[0] != signature[0])
            reset();
        configured = signature;

        const auto mode = std::clamp (s.mode, 1, 4);
        const float size = std::clamp (s.size * .01f, 0.0f, 1.0f);
        const float modeSize = mode == 1 ? .65f : mode == 2 ? 1.05f
                             : mode == 3 ? .75f : 1.38f;
        constexpr std::array<float, 8> baseMs {
            27.7f, 31.3f, 36.7f, 42.1f, 47.9f, 53.9f, 61.1f, 68.3f
        };
        const double rt60 = std::clamp (double (s.decaySeconds), 0.0, 30.0);
        for (int i = 0; i < 8; ++i)
        {
            const auto index = static_cast<std::size_t> (i);
            targetLength[index] = std::clamp (
                float (baseMs[index] * modeSize * (.45f + 1.80f * size) * sampleRate * .001),
                2.0f, float (lines[index].size() - 2));
            if (readLength[index] <= 0.0f)
                readLength[index] = targetLength[index];
            feedback[index] = rt60 < .001 ? 0.0f : float (std::exp (
                -6.907755278982137 * double (targetLength[index]) / (sampleRate * rt60)));
        }

        // LSampler's original FDN damping, warmth and open-sky diffusion.
        const double damper = std::clamp (double (s.damping) * .01, 0.0, 1.0);
        const double warmth = std::clamp (double (s.bassMultiplier), .5, 2.0);
        const double hz = 17000.0 * std::pow (240.0 / 17000.0, damper)
                            * std::pow (1.3 / warmth, .65);
        const double openSky = std::clamp ((double (s.xoverHz) - 50.0) / 950.0, 0.0, 1.0);
        diffusion = float (.56 + .44 * openSky);
        dampingAlpha = float (1.0 - std::exp (-6.283185307179586 * hz / sampleRate));
        lengthGlide = float (1.0 - std::exp (-1.0 / (.060 * sampleRate)));

        // Six early taps, identical timings/signs to LSampler FXEngine.
        predelayFrames = std::clamp (int (std::lround (s.predelayMs * .001 * sampleRate)),
                                    0, int (predelayLeft.size()) - 2);
        const float modeFactor = mode == 1 ? .50f : mode == 2 ? 1.0f
                               : mode == 3 ? .75f : 1.3f;
        earlySpacing = float (sampleRate * .001 * modeFactor * (3.0 + 22.0 * size));
        earlyAmount = std::clamp (s.early * .01f, 0.0f, 1.0f);
        const auto toneHz = 350.0 * std::pow (18000.0 / 350.0,
                                              double (std::clamp (s.tone, 0.0f, 100.0f)) * .01);
        toneAlpha = float (1.0 - std::exp (-6.283185307179586 * toneHz / sampleRate));
        sideWidth = std::clamp (s.width * .01f, 0.0f, 1.0f);
        wetGain = std::pow (10.0f, s.gainDb / 20.0f);
        gateEnabled = s.gateThresholdDb > -79.5f;
        gateThreshold = gateEnabled ? std::pow (10.0f, s.gateThresholdDb / 20.0f) : 0.0f;
        const auto decaySamples = float (sampleRate * .001 * s.gateDecayMs);
        gateDecayStep = decaySamples <= 1.0f ? 1.0f : 1.0f / decaySamples;
        // No eight-line processing on silent blocks with no live tail.
        // Allow 1.5x RT60 for the late decay to settle below -90 dB.
        tailBudgetFrames = static_cast<int> (sampleRate * (0.3 +
            std::clamp (double (s.decaySeconds), 0.0, 30.0) * 1.5
            + std::max (0.0, double (s.predelayMs)) * .001));
    }

    // Wet-only; callers preserve the dry bus and mix the send return.
    void process (float inputLeft, float inputRight, float& wetLeft, float& wetRight) noexcept
    {
        wetLeft = wetRight = 0.0f;
        if (lines[0].empty() || predelayLeft.empty())
            return;
        if (std::abs (inputLeft) > 1.0e-10f || std::abs (inputRight) > 1.0e-10f)
            tailFramesRemaining = tailBudgetFrames;
        else if (tailFramesRemaining > 0)
            --tailFramesRemaining;
        else
            return; // silent idle: no FDN work or new reverberant impulse
        predelayLeft[static_cast<std::size_t> (predelayWritePosition)] = inputLeft;
        predelayRight[static_cast<std::size_t> (predelayWritePosition)] = inputRight;
        const float preL = readPredelay (predelayLeft, float (predelayFrames));
        const float preR = readPredelay (predelayRight, float (predelayFrames));
        const float e1L = readPredelay (predelayLeft, float (predelayFrames) + earlySpacing);
        const float e1R = readPredelay (predelayRight, float (predelayFrames) + earlySpacing * 1.13f);
        const float e2L = readPredelay (predelayRight, float (predelayFrames) + earlySpacing * 2.37f);
        const float e2R = readPredelay (predelayLeft, float (predelayFrames) + earlySpacing * 2.87f);
        const float e3L = readPredelay (predelayLeft, float (predelayFrames) + earlySpacing * 3.73f);
        const float e3R = readPredelay (predelayRight, float (predelayFrames) + earlySpacing * 4.19f);
        if (++predelayWritePosition >= int (predelayLeft.size()))
            predelayWritePosition = 0;

        float tailL = 0.0f, tailR = 0.0f;
        processFdn (preL, preR, tailL, tailR);
        tailToneLow[0] += toneAlpha * (tailL - tailToneLow[0]);
        tailToneLow[1] += toneAlpha * (tailR - tailToneLow[1]);
        float revL = tailToneLow[0] * 1.80f
                   + earlyAmount * (.48f * e1L + .35f * e2L - .27f * e3L);
        float revR = tailToneLow[1] * 1.80f
                   + earlyAmount * (.48f * e1R + .35f * e2R - .27f * e3R);
        if (gateEnabled)
        {
            if (std::max (std::abs (inputLeft), std::abs (inputRight)) >= gateThreshold)
                gateLevel = 1.0f;
            else
                gateLevel = std::max (0.0f, gateLevel - gateDecayStep);
            revL *= gateLevel;
            revR *= gateLevel;
        }
        else
        {
            gateLevel = 1.0f;
        }
        const float mid = .5f * (revL + revR);
        const float side = .5f * (revL - revR) * sideWidth;
        wetLeft = wetGain * (mid + side);
        wetRight = wetGain * (mid - side);
        if (!std::isfinite (wetLeft)) wetLeft = 0.0f;
        if (!std::isfinite (wetRight)) wetRight = 0.0f;
    }

private:
    float readPredelay (const std::vector<float>& buffer, float distance) const noexcept
    {
        float pos = float (predelayWritePosition) - distance;
        const float capacity = float (buffer.size());
        // Capacity .45s safely holds even Cathedral's largest early offset.
        while (pos < 0.0f) pos += capacity;
        while (pos >= capacity) pos -= capacity;
        const int index = int (pos);
        const float f = pos - float (index);
        return buffer[static_cast<std::size_t> (index)]
             + f * (buffer[static_cast<std::size_t> ((index + 1) % int (buffer.size()))]
                    - buffer[static_cast<std::size_t> (index)]);
    }

    void processFdn (float inL, float inR, float& wetL, float& wetR) noexcept
    {
        constexpr float norm = .3535533905932738f;
        std::array<float, 8> taps {};
        for (int i = 0; i < 8; ++i)
        {
            const auto index = static_cast<std::size_t> (i);
            auto& line = lines[index];
            readLength[index] += (targetLength[index] - readLength[index]) * lengthGlide;
            float pos = float (writePosition) - readLength[index];
            if (pos < 0.0f) pos += float (line.size());
            const int a = int (pos);
            const float f = pos - float (a);
            const float read = line[static_cast<std::size_t> (a)]
                + f * (line[static_cast<std::size_t> ((a + 1) % int (line.size()))]
                       - line[static_cast<std::size_t> (a)]);
            damped[index] += dampingAlpha * (read - damped[index]);
            taps[index] = damped[index] * diffusion + read * (1.0f - diffusion);
        }
        wetL = norm * (taps[0] + taps[1] - taps[2] - taps[3]
                      + taps[4] + taps[5] - taps[6] - taps[7]);
        wetR = norm * (taps[0] - taps[1] + taps[2] - taps[3]
                      - taps[4] + taps[5] - taps[6] + taps[7]);
        std::array<float, 8> mixed = taps;
        for (int stride = 1; stride < 8; stride *= 2)
            for (int block = 0; block < 8; block += 2 * stride)
                for (int j = 0; j < stride; ++j)
                {
                    const float a = mixed[static_cast<std::size_t> (block + j)];
                    const float b = mixed[static_cast<std::size_t> (block + j + stride)];
                    mixed[static_cast<std::size_t> (block + j)] = a + b;
                    mixed[static_cast<std::size_t> (block + j + stride)] = a - b;
                }
        for (int i = 0; i < 8; ++i)
        {
            const float injected = i & 1 ? inR : inL;
            const float next = .24f * injected + norm * feedback[static_cast<std::size_t> (i)]
                             * mixed[static_cast<std::size_t> (i)];
            lines[static_cast<std::size_t> (i)][static_cast<std::size_t> (writePosition)] =
                std::clamp (next, -8.0f, 8.0f);
        }
        if (++writePosition >= int (lines[0].size())) writePosition = 0;
    }

    double sampleRate = 44100.0;
    std::array<std::vector<float>, 8> lines;
    std::vector<float> predelayLeft, predelayRight;
    std::array<float, 8> damped {}, readLength {}, targetLength {}, feedback {};
    std::array<float, 2> tailToneLow {};
    std::array<float, 13> configured {};
    float dampingAlpha = 1.0f, lengthGlide = .001f, diffusion = 1.0f;
    float earlySpacing = 0.0f, earlyAmount = 0.2f, toneAlpha = 1.0f;
    float sideWidth = 1.0f, wetGain = 1.0f, gateLevel = 0.0f;
    float gateThreshold = 0.0f, gateDecayStep = 1.0f;
    int writePosition = 0, predelayWritePosition = 0, predelayFrames = 0;
    int tailFramesRemaining = 0, tailBudgetFrames = 0;
    bool gateEnabled = false;
};
} // namespace ljuno
