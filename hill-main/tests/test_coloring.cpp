#include "coloring/coloring.hpp"

#include <iostream>

int main() {
    std::string str = "message";
    std::cout << Hill::ColorizedString(str.c_str(), Hill::Colors::Black) << "\n";
    std::cout << Hill::ColorizedString(str.c_str(), Hill::Colors::Red) << "\n";
    std::cout << Hill::ColorizedString(str.c_str(), Hill::Colors::Green) << "\n";
    std::cout << Hill::ColorizedString(str.c_str(), Hill::Colors::Yellow) << "\n";
    std::cout << Hill::ColorizedString(str.c_str(), Hill::Colors::Blue) << "\n";
    std::cout << Hill::ColorizedString(str.c_str(), Hill::Colors::Magenta) << "\n";
    std::cout << Hill::ColorizedString(str.c_str(), Hill::Colors::Cyan) << "\n";
    std::cout << Hill::ColorizedString(str.c_str(), Hill::Colors::White) << "\n";
    
    return 0;
}
