#ifndef SOUND_H
#define SOUND_H

struct ma_sound;

namespace CC
{
    class Sound
    {
    public:
        Sound();
        ~Sound();

        bool LoadFromFile(const char* filePath, bool isLooping, bool isStreamed);
        void Start();
        void Stop();
        bool IsPlaying() const;
        void SetVolume(float volume);

    private:
        ma_sound* maSound;
        bool isLoaded;
    };
}

#endif // SOUND_H
