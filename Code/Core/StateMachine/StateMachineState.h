#ifndef STATEMACHINESTATE_H
#define STATEMACHINESTATE_H

namespace CC
{
    class StateMachine;
}

class StateMachineState
{
    public:
        StateMachineState() {};
        virtual ~StateMachineState() {};

        virtual void Init() {};
        virtual void Update() {};
        virtual void Shutdown() {};

        // Whether this state's scene can be entered in a headset. False by
        // default: a state that has not thought about stereo, a tracked head
        // and a scene in metres does not support XR, and offering it would
        // put the user in a headset looking at something authored for a
        // window. A state that answers true also registers the XR screen its
        // pause menu routes to.
        bool IsXrSupported() const { return isXrSupported; }

        // ========================
        // Pause
        // ========================
        // Whether this state has a world to freeze. False by default: a state
        // like the audio tracker has no pause at all.
        bool IsPausable() const { return isPausable; }
        bool IsPaused() const { return isPaused; }

        // Freezes or resumes the Simulation — the scene's pause flag and the
        // Simulation clock together — then hands over to OnPaused or
        // OnResumed for everything the state shows and routes. Acts only on a
        // change.
        void SetPaused(bool doPause);
        void TogglePause();

    protected:
        bool isXrSupported = false;
        bool isPausable = false;

        // The state's own half of pausing: its action-map context, both input
        // targets, its screens and its panels.
        virtual void OnPaused() {};
        virtual void OnResumed() {};

    private:
        // A swap clears the leaving state's pause without calling it back:
        // its screens and panels are about to be destroyed by its own
        // Shutdown, and the state is reused the next time it is entered.
        friend class CC::StateMachine;

        bool isPaused = false;
};

#endif // STATEMACHINESTATE_H
