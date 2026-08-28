#ifndef DEVUIVIEW_H
#define DEVUIVIEW_H

namespace CC
{
    class DevUiView
    {
    public:
        virtual ~DevUiView() = default;

        virtual void Draw() = 0;
        virtual const char* GetName() const = 0;

        void SetVisible(bool _isVisible) { isVisible = _isVisible; }
        bool IsVisible() const { return isVisible; }
        void ToggleVisible() { isVisible = !isVisible; }

    protected:
        bool isVisible = false;
    };
}

#endif // DEVUIVIEW_H
