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
    private:
};

#endif // STATEMACHINESTATE_H
