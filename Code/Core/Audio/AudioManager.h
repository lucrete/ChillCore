#ifndef AUDIOMANAGER_H
#define AUDIOMANAGER_H

#include <vector>

struct ma_engine;
struct ma_sound;

namespace CC
{
    class Sound;

    enum class SfxId
    {
        UiMove,
        UiAdvance,
        UiBack,
        Count
    };

    class AudioManager
    {
    public:
        AudioManager();
        virtual ~AudioManager();

        static AudioManager* Get();

        void Update();

        void LoadSfx(SfxId id, const char* filePath);
        void PlaySfx(SfxId id);
        void PlaySfx(const char* filePath, float volume = 1.0f);

        Sound* PlayMusic(const char* filePath, bool isLooping);
        void StopMusic(Sound* sound);
        void SetMasterVolume(float volume);

        ma_engine* GetEngine() { return engine; }

    private:
        static AudioManager* instance;

        ma_engine* engine;
        std::vector<ma_sound*> activeSfx;
        std::vector<ma_sound*> preloadedSfx;
    };
}

#endif // AUDIOMANAGER_H
