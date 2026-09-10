#ifndef COMPONENT_H
#define COMPONENT_H

namespace CC
{
    class SceneObject;

    class Component
    {
    public:
        Component();
        virtual ~Component();

        virtual void Init() {}
        virtual void Update() {}
        virtual void Shutdown() {}

        void SetOwner(SceneObject* _owner);
        SceneObject* GetOwner() const { return owner; }

        // Virtual because a component can own state outside itself that has
        // to stop with it — a UI panel's surface skips its render.
        virtual void SetEnabled(bool _isEnabled) { isEnabled = _isEnabled; }
        bool IsEnabled() const { return isEnabled; }

        void SetPauseable(bool _isPauseable) { isPauseable = _isPauseable; }
        bool IsPauseable() const { return isPauseable; }

        virtual const char* GetTypeName() const = 0;

    protected:
        SceneObject* owner = nullptr;
        bool isEnabled = true;
        bool isPauseable = false;
    };
}

#endif // COMPONENT_H
