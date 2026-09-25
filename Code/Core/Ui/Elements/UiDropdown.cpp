#include "UiDropdown.h"

namespace CC
{
    const std::string UiDropdown::EMPTY_STRING;

    UiDropdown::UiDropdown()
        : UiElement(UiElementType::Dropdown)
    {
        isNavigable = true;
    }

    UiDropdown::~UiDropdown()
    {
    }

    void UiDropdown::AddOption(const std::string& option)
    {
        options.push_back(option);
    }

    void UiDropdown::SetSelectedOption(int index)
    {
        if (index >= 0 && index < (int)options.size() && index != selectedOption)
        {
            selectedOption = index;
            MarkSurfaceDirty();
        }
    }

    void UiDropdown::SetExpanded(bool expanded)
    {
        if (isExpanded != expanded)
        {
            isExpanded = expanded;
            MarkSurfaceDirty();
        }
    }

    void UiDropdown::SetHoveredOption(int index)
    {
        if (hoveredOption != index)
        {
            hoveredOption = index;
            MarkSurfaceDirty();
        }
    }

    const std::string& UiDropdown::GetSelectedText() const
    {
        if (selectedOption >= 0 && selectedOption < (int)options.size())
        {
            return options[selectedOption];
        }
        return EMPTY_STRING;
    }
}
