#include "AudioManager.h"

#include "Sound.h"
#include "PrintManager.h"
#include "CCAssert.h"
#include "miniaudio.h"

namespace CC
{
    AudioManager* AudioManager::instance = nullptr;

    AudioManager::AudioManager()
        : engine(nullptr)
    {
        CC_ASSERT(instance == nullptr, "AudioManager already created");
        instance = this;

        engine = new ma_engine();
        ma_engine_config config = ma_engine_config_init();
        ma_result result = ma_engine_init(&config, engine);
        if (result != MA_SUCCESS)
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "AudioManager: ma_engine_init failed");
            delete engine;
            engine = nullptr;
        }

        preloadedSfx.resize((size_t)SfxId::Count, nullptr);
    }

    AudioManager::~AudioManager()
    {
        for (size_t i = 0; i < activeSfx.size(); i++)
        {
            ma_sound_uninit(activeSfx[i]);
            delete activeSfx[i];
        }
        activeSfx.clear();

        for (size_t i = 0; i < preloadedSfx.size(); i++)
        {
            if (preloadedSfx[i] != nullptr)
            {
                ma_sound_uninit(preloadedSfx[i]);
                delete preloadedSfx[i];
                preloadedSfx[i] = nullptr;
            }
        }
        preloadedSfx.clear();

        if (engine != nullptr)
        {
            ma_engine_uninit(engine);
            delete engine;
            engine = nullptr;
        }
        instance = nullptr;
    }

    AudioManager* AudioManager::Get()
    {
        CC_ASSERT(instance != nullptr, "AudioManager not created yet");
        return instance;
    }

    // Reap fire-and-forget SFX that have finished playing. Called once per
    // frame from CoreMain so memory does not accumulate across long sessions.
    void AudioManager::Update()
    {
        size_t writeIndex = 0;
        for (size_t readIndex = 0; readIndex < activeSfx.size(); readIndex++)
        {
            ma_sound* sound = activeSfx[readIndex];
            if (ma_sound_at_end(sound))
            {
                ma_sound_uninit(sound);
                delete sound;
            }
            else
            {
                activeSfx[writeIndex] = sound;
                writeIndex++;
            }
        }
        activeSfx.resize(writeIndex);
    }

    void AudioManager::LoadSfx(SfxId id, const char* filePath)
    {
        const size_t index = (size_t)id;
        if (engine != nullptr && index < preloadedSfx.size())
        {
            if (preloadedSfx[index] != nullptr)
            {
                ma_sound_uninit(preloadedSfx[index]);
                delete preloadedSfx[index];
                preloadedSfx[index] = nullptr;
            }

            ma_sound* prototype = new ma_sound();
            ma_uint32 flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION;
            ma_result result = ma_sound_init_from_file(engine, filePath, flags, NULL, NULL, prototype);
            if (result == MA_SUCCESS)
            {
                preloadedSfx[index] = prototype;
            }
            else
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "AudioManager::LoadSfx failed to load file");
                delete prototype;
            }
        }
    }

    // Plays a preloaded SFX. Each call clones the preloaded prototype so
    // rapid retriggers overlap rather than truncate. Decoded audio data is
    // shared with the prototype — the clone owns only its playback state.
    void AudioManager::PlaySfx(SfxId id)
    {
        const size_t index = (size_t)id;
        if (engine != nullptr && index < preloadedSfx.size() && preloadedSfx[index] != nullptr)
        {
            ma_sound* clone = new ma_sound();
            ma_uint32 flags = MA_SOUND_FLAG_NO_SPATIALIZATION;
            ma_result result = ma_sound_init_copy(engine, preloadedSfx[index], flags, NULL, clone);
            if (result == MA_SUCCESS)
            {
                ma_sound_start(clone);
                activeSfx.push_back(clone);
            }
            else
            {
                delete clone;
            }
        }
    }

    void AudioManager::PlaySfx(const char* filePath, float volume)
    {
        if (engine != nullptr)
        {
            ma_sound* sound = new ma_sound();
            ma_result result = ma_sound_init_from_file(engine, filePath, MA_SOUND_FLAG_DECODE, NULL, NULL, sound);
            if (result == MA_SUCCESS)
            {
                ma_sound_set_volume(sound, volume);
                ma_sound_start(sound);
                activeSfx.push_back(sound);
            }
            else
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "AudioManager::PlaySfx failed to load file");
                delete sound;
            }
        }
    }

    Sound* AudioManager::PlayMusic(const char* filePath, bool isLooping)
    {
        Sound* sound = nullptr;
        if (engine != nullptr)
        {
            sound = new Sound();
            bool isLoaded = sound->LoadFromFile(filePath, isLooping, true);
            if (isLoaded)
            {
                sound->Start();
            }
            else
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "AudioManager::PlayMusic failed to load file");
                delete sound;
                sound = nullptr;
            }
        }
        return sound;
    }

    void AudioManager::StopMusic(Sound* sound)
    {
        if (sound != nullptr)
        {
            sound->Stop();
            delete sound;
        }
    }

    void AudioManager::SetMasterVolume(float volume)
    {
        if (engine != nullptr)
        {
            ma_engine_set_volume(engine, volume);
        }
    }
}
