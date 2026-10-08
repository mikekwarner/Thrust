#include "Audio.h"
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace Audio {

static Sound sndLaser;
static Sound sndExplosion;
static Sound sndFuelPickup;
static Sound sndCountdownBeep;
static Sound sndTeleport;
static Sound sndEngine;
static Sound sndShield;

static bool enginePlaying = false;
static bool shieldPlaying = false;

static Sound GenerateSound(int sampleRate, float duration, auto generator) {
    int frameCount = static_cast<int>(sampleRate * duration);
    std::vector<short> samples(frameCount);
    
    for (int i = 0; i < frameCount; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float val = generator(t, duration, i);
        if (val > 1.0f) val = 1.0f;
        if (val < -1.0f) val = -1.0f;
        samples[i] = static_cast<short>(val * 32767.0f);
    }
    
    Wave wave;
    wave.frameCount = frameCount;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = samples.data();
    
    Sound s = LoadSoundFromWave(wave);
    return s;
}

void Init() {
    InitAudioDevice();
    const int sr = 44100;
    
    // Fire / Laser Cannon: Punchy arcade plasma-laser zap with initial discharge snap,
    // true phase-integrated exponential pitch drop (1650 Hz -> 135 Hz), and sub-octave body (0.16s)
    {
        const float duration = 0.16f;
        const int frameCount = static_cast<int>(sr * duration);
        std::vector<short> samples(frameCount);
        unsigned int rng = 0x4C415352u;
        float phaseMain = 0.0f;
        float phaseSub  = 0.0f;
        float lpSnap    = 0.0f;

        for (int i = 0; i < frameCount; ++i) {
            float t = static_cast<float>(i) / sr;
            float frac = t / duration;

            // Steep exponential frequency sweep from 1650 Hz down to 135 Hz + subtle FM laser bite
            float baseFreq = 135.0f + 1515.0f * std::exp(-t * 23.0f);
            float fmBite = 1.0f + 0.055f * std::sin(2.0f * 3.1415926535f * 55.0f * t);
            float freq = baseFreq * fmBite;

            phaseMain += freq / sr;
            phaseSub  += (freq * 0.5f) / sr;

            float pMain = std::fmod(phaseMain, 1.0f);
            // Asymmetric pulse wave (duty cycle sweeps from 0.32 -> 0.50) + warm sine fundamental
            float duty = 0.32f + 0.18f * frac;
            float pulse = (pMain < duty) ? 1.0f : -1.0f;
            float sineMain = std::sin(2.0f * 3.1415926535f * phaseMain);
            float sineSub  = std::sin(2.0f * 3.1415926535f * phaseSub);

            // Initial high-voltage plasma discharge crack (first 20ms)
            rng = rng * 1664525u + 1013904223u;
            float noise = (static_cast<float>((rng >> 8) & 0xFFFF) / 32767.5f) - 1.0f;
            lpSnap += 0.38f * (noise - lpSnap);
            float snapEnv = std::exp(-t * 95.0f);

            // Fast-attack, punchy decay envelope
            float attack = std::min(1.0f, t / 0.002f);
            float decay  = std::pow(1.0f - frac, 1.45f);
            float env    = attack * decay;

            float raw = ((pulse * 0.42f + sineMain * 0.38f + sineSub * 0.32f) * env + lpSnap * snapEnv * 0.55f) * 0.72f;
            float val = std::tanh(raw);
            samples[i] = static_cast<short>(std::clamp(val, -1.0f, 1.0f) * 32767.0f);
        }

        Wave wave;
        wave.frameCount = frameCount;
        wave.sampleRate = sr;
        wave.sampleSize = 16;
        wave.channels = 1;
        wave.data = samples.data();
        sndLaser = LoadSoundFromWave(wave);
    }
    
    // Explosion: Dramatic multi-stage detonation (shockwave bass punch + sweeping cavern rumble + secondary debris crackles, 1.15s)
    {
        const float duration = 1.15f;
        const int frameCount = static_cast<int>(sr * duration);
        std::vector<short> samples(frameCount);
        unsigned int rng = 0x424F4F4Du;
        float lpSub = 0.0f;
        float lpMid = 0.0f;
        float lpHigh = 0.0f;
        float shockPhase = 0.0f;

        for (int i = 0; i < frameCount; ++i) {
            float t = static_cast<float>(i) / sr;
            float frac = t / duration;

            rng = rng * 1664525u + 1013904223u;
            float w = (static_cast<float>((rng >> 8) & 0xFFFF) / 32767.5f) - 1.0f;

            // Filter cutoffs sweep downward as the fireball expands into a deep cavern roar
            float alphaSub  = 0.014f + 0.028f * std::exp(-t * 4.5f);
            float alphaMid  = 0.055f + 0.160f * std::exp(-t * 5.0f);
            float alphaHigh = 0.220f + 0.450f * std::exp(-t * 8.0f);

            lpSub  += alphaSub  * (w - lpSub);
            lpMid  += alphaMid  * (w - lpMid);
            lpHigh += alphaHigh * (w - lpHigh);

            // 1. Initial shockwave sub-bass punch (135 Hz -> 28 Hz pitch drop)
            float shockFreq = 28.0f + 107.0f * std::exp(-t * 18.0f);
            shockPhase += shockFreq / sr;
            float shockWave = std::sin(2.0f * 3.1415926535f * shockPhase) * std::exp(-t * 8.5f);

            // 2. Primary + secondary detonation envelopes (secondary blasts at t=0.09s and t=0.21s)
            float mainEnv = std::exp(-t * 3.2f);
            float sec1 = 0.65f * std::exp(-std::pow((t - 0.09f) / 0.045f, 2.0f));
            float sec2 = 0.45f * std::exp(-std::pow((t - 0.21f) / 0.065f, 2.0f));
            float totalEnv = (mainEnv + sec1 + sec2) * (1.0f - frac * 0.65f);

            // 3. Low-frequency turbulence modulation for classic 8-bit/arcade explosion crunch
            float crunch = 0.78f + 0.22f * std::sin(2.0f * 3.1415926535f * (38.0f - 18.0f * frac) * t);

            float rumble = lpSub * 2.4f + (lpMid - lpSub) * 1.45f;
            float crackle = (lpHigh - lpMid) * 0.85f * std::exp(-t * 5.5f);
            float raw = (shockWave * 0.95f + (rumble + crackle) * totalEnv * crunch) * 1.35f;

            // Soft saturation for loud, punchy, distortion-free impact
            float val = std::tanh(raw);
            samples[i] = static_cast<short>(std::clamp(val, -1.0f, 1.0f) * 32767.0f);
        }

        Wave wave;
        wave.frameCount = frameCount;
        wave.sampleRate = sr;
        wave.sampleSize = 16;
        wave.channels = 1;
        wave.data = samples.data();
        sndExplosion = LoadSoundFromWave(wave);
    }
    
    // Fuel Pickup: 3-note ascending arpeggio (C5, E5, G5, each 0.08s)
    sndFuelPickup = GenerateSound(sr, 0.24f, [](float t, float, int) {
        float freq = 523.25f; // C5
        if (t >= 0.08f && t < 0.16f) freq = 659.25f; // E5
        else if (t >= 0.16f) freq = 783.99f; // G5
        float phase = freq * t;
        float sq = (std::fmod(phase, 1.0f) > 0.5f) ? 1.0f : -1.0f;
        return sq * 0.35f;
    });
    
    // Countdown Beep: Clean 1000 Hz square wave (0.08s)
    sndCountdownBeep = GenerateSound(sr, 0.08f, [](float t, float d, int) {
        float phase = 1000.0f * t;
        float sq = (std::fmod(phase, 1.0f) > 0.5f) ? 1.0f : -1.0f;
        float env = 1.0f - (t / d) * 0.5f;
        return sq * env * 0.4f;
    });
    
    // Spawn / Teleport: Light, echoey crystalline "Ping" lasting the full 1.20s spawn process
    {
        const float duration = 1.20f;
        const int frameCount = static_cast<int>(sr * duration);
        std::vector<short> samples(frameCount);
        const float twoPi = 2.0f * 3.1415926535f;

        // Dual reverberant delay lines (47ms and 73ms) for lush cavern diffusion around the discrete echo taps
        const int delay1Len = static_cast<int>(sr * 0.047f);
        const int delay2Len = static_cast<int>(sr * 0.073f);
        std::vector<float> delayBuf1(delay1Len, 0.0f);
        std::vector<float> delayBuf2(delay2Len, 0.0f);
        int dIdx1 = 0;
        int dIdx2 = 0;
        float lpEcho1 = 0.0f;
        float lpEcho2 = 0.0f;

        for (int i = 0; i < frameCount; ++i) {
            float t = static_cast<float>(i) / sr;

            // Multi-tap echoing pings spaced every 0.155s across the 1.20s spawn duration,
            // with each reflection gently softening its high-frequency overtones
            float dryAndTaps = 0.0f;
            const int numTaps = 7;
            const float tapSpacing = 0.155f;
            for (int tap = 0; tap < numTaps; ++tap) {
                float dtTap = t - tap * tapSpacing;
                if (dtTap < 0.0f) break;

                float tapGain = std::pow(0.66f, static_cast<float>(tap));
                float attack  = std::min(1.0f, dtTap / 0.0018f);
                float envMain = attack * std::exp(-dtTap * 7.2f);
                // Higher overtones damp progressively faster on later echoes for natural acoustic depth
                float hfDamp  = std::pow(0.72f, static_cast<float>(tap));
                float envHarm = attack * std::exp(-dtTap * (16.0f + tap * 3.5f)) * hfDamp;
                float envStrike = (tap == 0) ? (attack * std::exp(-dtTap * 55.0f)) : 0.0f;

                // Subtle chorus detune on echoes for an airy, spacious shimmer
                float detune = (tap == 0) ? 1.0f : (1.0f + ((tap % 2 == 0) ? 0.0012f : -0.0012f));
                float f1 = std::sin(twoPi * (1760.0f * detune) * dtTap);
                float f2 = std::sin(twoPi * (2637.0f * detune) * dtTap);
                float f3 = std::sin(twoPi * (3520.0f * detune) * dtTap);
                float strike = std::sin(twoPi * 5274.0f * dtTap);

                float ping = f1 * envMain * 0.65f
                           + f2 * envHarm * 0.22f
                           + f3 * envHarm * 0.13f
                           + strike * envStrike * 0.10f;
                dryAndTaps += ping * tapGain;
            }

            // Feed into damped reverberant delay lines for a smooth, continuous echo tail
            float del1 = delayBuf1[dIdx1];
            float del2 = delayBuf2[dIdx2];
            lpEcho1 += 0.35f * (del1 - lpEcho1);
            lpEcho2 += 0.28f * (del2 - lpEcho2);

            delayBuf1[dIdx1] = dryAndTaps + lpEcho2 * 0.42f;
            delayBuf2[dIdx2] = dryAndTaps + lpEcho1 * 0.42f;
            if (++dIdx1 >= delay1Len) dIdx1 = 0;
            if (++dIdx2 >= delay2Len) dIdx2 = 0;

            // Smooth fade-out over the final 80ms so the 1.20s tail ends cleanly
            float endFade = std::clamp((duration - t) / 0.08f, 0.0f, 1.0f);
            float mix = (dryAndTaps * 0.78f + (lpEcho1 + lpEcho2) * 0.34f) * endFade * 0.44f;
            samples[i] = static_cast<short>(std::clamp(mix, -1.0f, 1.0f) * 32767.0f);
        }

        Wave wave;
        wave.frameCount = frameCount;
        wave.sampleRate = sr;
        wave.sampleSize = 16;
        wave.channels = 1;
        wave.data = samples.data();
        sndTeleport = LoadSoundFromWave(wave);
    }
    
    // Engine thruster: pressurized rocket exhaust hiss (band-shaped noise with subtle combustion rumble, seamless 1.0s loop)
    {
        const float duration = 1.0f;
        const int frameCount = static_cast<int>(sr * duration);
        std::vector<float> rawNoise(frameCount);
        unsigned int rngState = 0x54485255u;
        for (int i = 0; i < frameCount; ++i) {
            rngState = rngState * 1664525u + 1013904223u;
            rawNoise[i] = (static_cast<float>((rngState >> 8) & 0xFFFF) / 32767.5f) - 1.0f;
        }

        std::vector<short> samples(frameCount);
        float lpFast = 0.0f;
        float lpSlow = 0.0f;
        const float alphaFast = 0.52f; // Crisp upper-mid/high rocket hiss cutoff
        const float alphaSlow = 0.045f; // Warm low-end exhaust body cutoff

        // Pre-warm filter on tail of buffer so loop wrap at sample 0 is completely seamless
        for (int i = frameCount - 256; i < frameCount; ++i) {
            float w = rawNoise[i];
            lpFast += alphaFast * (w - lpFast);
            lpSlow += alphaSlow * (w - lpSlow);
        }

        for (int i = 0; i < frameCount; ++i) {
            float t = static_cast<float>(i) / sr;
            float w = rawNoise[i];
            lpFast += alphaFast * (w - lpFast);
            lpSlow += alphaSlow * (w - lpSlow);

            // High-passed hiss component + subtle low turbulence
            float hiss = (w * 0.35f + lpFast * 0.65f) - lpSlow * 0.72f;
            float rumble = lpSlow * 0.22f;
            float flutter = 0.92f + 0.08f * std::sin(2.0f * 3.1415926535f * 32.0f * t);
            float val = std::clamp((hiss + rumble) * flutter * 0.48f, -1.0f, 1.0f);
            samples[i] = static_cast<short>(val * 32767.0f);
        }

        Wave wave;
        wave.frameCount = frameCount;
        wave.sampleRate = sr;
        wave.sampleSize = 16;
        wave.channels = 1;
        wave.data = samples.data();
        sndEngine = LoadSoundFromWave(wave);
    }

    // Shield / Tractor Beam: Seamless 1.0s pulsating electromagnetic force-field hum
    // Uses exact integer cycle counts over 1.0s so loop wrap is 100% phase-continuous and click-free
    {
        const float duration = 1.0f;
        const int frameCount = static_cast<int>(sr * duration);
        std::vector<short> samples(frameCount);
        const float twoPi = 2.0f * 3.1415926535f;

        for (int i = 0; i < frameCount; ++i) {
            float t = static_cast<float>(i) / sr;

            // 15 Hz rotary force-field warble (15 exact cycles in 1.0s)
            float lfo15 = std::sin(twoPi * 15.0f * t);
            float lfo30 = std::cos(twoPi * 30.0f * t);

            // Phase-modulated electromagnetic carrier (110 Hz fundamental + 165 Hz fifth + 330 Hz harmonic)
            // Analytic integral of frequency modulation ensures exact phase match at t=0 and t=1.0
            float pm = 0.85f * std::sin(twoPi * 15.0f * t);
            float carrier110 = std::sin(twoPi * 110.0f * t + pm);
            float fifth165   = std::sin(twoPi * 165.0f * t + pm * 1.5f);
            float upper330   = std::sin(twoPi * 330.0f * t - pm * 2.0f);
            float shimmer660 = std::sin(twoPi * 660.0f * t + 0.6f * lfo30);

            // Soft-clipped resonant pulse character + rhythmic force-field throb
            float fieldWave = std::tanh((carrier110 * 0.55f + fifth165 * 0.35f + upper330 * 0.25f) * 1.8f);
            float shimmer   = shimmer660 * (0.18f + 0.12f * lfo15);
            float ampMod    = 0.78f + 0.22f * lfo15;

            float val = std::clamp((fieldWave * 0.78f + shimmer) * ampMod * 0.38f, -1.0f, 1.0f);
            samples[i] = static_cast<short>(val * 32767.0f);
        }

        Wave wave;
        wave.frameCount = frameCount;
        wave.sampleRate = sr;
        wave.sampleSize = 16;
        wave.channels = 1;
        wave.data = samples.data();
        sndShield = LoadSoundFromWave(wave);
    }
}

void Close() {
    UnloadSound(sndLaser);
    UnloadSound(sndExplosion);
    UnloadSound(sndFuelPickup);
    UnloadSound(sndCountdownBeep);
    UnloadSound(sndTeleport);
    UnloadSound(sndEngine);
    UnloadSound(sndShield);
    CloseAudioDevice();
}

void PlayLaser() {
    PlaySound(sndLaser);
}

void PlayExplosion() {
    PlaySound(sndExplosion);
}

void PlayFuelPickup() {
    PlaySound(sndFuelPickup);
}

void PlayCountdownBeep() {
    PlaySound(sndCountdownBeep);
}

void PlayTeleport() {
    PlaySound(sndTeleport);
}

void SetEngineThrust(bool active) {
    if (active) {
        if (!enginePlaying || !IsSoundPlaying(sndEngine)) {
            PlaySound(sndEngine);
            enginePlaying = true;
        }
    } else {
        if (enginePlaying) {
            StopSound(sndEngine);
            enginePlaying = false;
        }
    }
}

void SetShieldSound(bool active) {
    if (active) {
        if (!shieldPlaying || !IsSoundPlaying(sndShield)) {
            PlaySound(sndShield);
            shieldPlaying = true;
        }
    } else {
        if (shieldPlaying) {
            StopSound(sndShield);
            shieldPlaying = false;
        }
    }
}

void Update() {
    if (enginePlaying && !IsSoundPlaying(sndEngine)) {
        PlaySound(sndEngine);
    }
    if (shieldPlaying && !IsSoundPlaying(sndShield)) {
        PlaySound(sndShield);
    }
}

} // namespace Audio
