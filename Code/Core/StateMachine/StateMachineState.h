#ifndef STATEMACHINESTATE_H
#define STATEMACHINESTATE_H

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

    protected:
        bool isXrSupported = false;
};

#endif // STATEMACHINESTATE_H
