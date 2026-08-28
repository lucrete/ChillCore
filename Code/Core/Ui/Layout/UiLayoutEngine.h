#ifndef UILAYOUTENGINE_H
#define UILAYOUTENGINE_H

namespace CC
{
    class UiElement;

    class UiLayoutEngine
    {
    public:
        static void ComputeLayout(UiElement* root, int screenWidth, int screenHeight);

    private:
        static float scaleFactor;

        static void MeasurePass(UiElement* element, float parentWidth, float parentHeight);
        static void ArrangePass(UiElement* element, float parentX, float parentY,
            float parentWidth, float parentHeight);

        static void InvalidateTextCaches(UiElement* element);

        static float ResolveWidth(UiElement* element, float parentWidth);
        static float ResolveHeight(UiElement* element, float parentHeight);
        static float ClampDimension(float value, float minVal, float maxVal,
            float parentSize, bool isMinAuto, bool isMaxAuto);
    };
}

#endif // UILAYOUTENGINE_H
