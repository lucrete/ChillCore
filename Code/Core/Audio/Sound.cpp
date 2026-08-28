#include "Sound.h"

#include "AudioManager.h"
#include "miniaudio.h"

namespace CC
{
    Sound::Sound()
        : maSound(nullptr)
        , isLoaded(false)
    {
    }

    Sound::~Sound()
    {
        if (isLoaded && maSound != nullptr)
        {
            ma_sound_uninit(maSound);
        }
        delete maSound;
        maSound = nullptr;
    }

    bool Sound::LoadFromFile(const char* filePath, bool isLooping, bool isStreamed)
    {
        bool isOk = false;
        ma_engine* engine = AudioManager::Get()->GetEngine();
        if (engine != nullptr)
        {
            maSound = new ma_sound();
            ma_uint32 flags = isStreamed ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE;
            ma_result result = ma_sound_init_from_file(engine, filePath, flags, NULL, NULL, maSound);
            if (result == MA_SUCCESS)
            {
                ma_sound_set_looping(maSound, isLooping ? MA_TRUE : MA_FALSE);
                isLoaded = true;
                isOk = true;
            }
            else
            {
                delete maSound;
                maSound = nullptr;
            }
        }
        return isOk;
    }

    void Sound::Start()
    {
        if (isLoaded && maSound != nullptr)
        {
            ma_sound_start(maSound);
        }
    }

    void Sound::Stop()
    {
        if (isLoaded && maSound != nullptr)
        {
            ma_sound_stop(maSound);
        }
    }

    bool Sound::IsPlaying() const
    {
        bool result = false;
        if (isLoaded && maSound != nullptr)
        {
            result = (ma_sound_is_playing(maSound) == MA_TRUE);
        }
        return result;
    }

    void Sound::SetVolume(float volume)
    {
        if (isLoaded && maSound != nullptr)
        {
            ma_sound_set_volume(maSound, volume);
        }
    }
}
