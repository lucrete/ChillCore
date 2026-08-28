#ifndef UIDROPDOWN_H
#define UIDROPDOWN_H

#include <string>
#include <vector>
#include "UiElement.h"

namespace CC
{
    class UiDropdown : public UiElement
    {
    public:
        UiDropdown();
        virtual ~UiDropdown();

        void AddOption(const std::string& option);
        const std::vector<std::string>& GetOptions() const { return options; }

        int GetSelectedOption() const { return selectedOption; }
        void SetSelectedOption(int index);
        const std::string& GetSelectedText() const;

        bool IsExpanded() const { return isExpanded; }
        void SetExpanded(bool expanded) { isExpanded = expanded; }
        void ToggleExpanded() { isExpanded = !isExpanded; }

        int GetHoveredOption() const { return hoveredOption; }
        void SetHoveredOption(int index) { hoveredOption = index; }

    private:
        std::vector<std::string> options;
        int selectedOption = 0;
        int hoveredOption = -1;
        bool isExpanded = false;

        static const std::string EMPTY_STRING;
    };
}

#endif // UIDROPDOWN_H
