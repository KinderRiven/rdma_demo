#ifndef __HILL__COLORING__COLORING__
#define __HILL__COLORING__COLORING__
#include <string>

namespace Hill {
    enum class Colors {
        Black,
        Red,
        Green,
        Yellow,
        Blue,  
        Magenta,
        Cyan, 
        White    
    };
    
    class ColorizedString {
    public:
        ColorizedString() = default;
        ColorizedString(const ColorizedString &) = default;
        ColorizedString(ColorizedString &&) = default;
        ~ColorizedString() = default;

        ColorizedString(const std::string &str, enum Colors color = Colors::White) {
            initialize(str.c_str(), color);
        }
        
        ColorizedString(const char *in, enum Colors color = Colors::White) {
            initialize(in, color);
        }
        
    private:
        void initialize(const char *in, enum Colors color) {
            colored.assign(in);
            
            switch(color){
            case Colors::Black:
                colored.insert(0, "\033[1;30m");
                break;
            case Colors::Red:
                colored.insert(0, "\033[1;31m");
                break;
            case Colors::Green:
                colored.insert(0, "\033[1;32m");
                break;
            case Colors::Yellow:
                colored.insert(0, "\033[1;33m");
                break;
            case Colors::Blue:
                colored.insert(0, "\033[1;34m");
                break;
            case Colors::Magenta:
                colored.insert(0, "\033[1;35m");
                break;
            case Colors::Cyan:
                colored.insert(0, "\033[1;36m");
                break;
            case Colors::White:
                // fall through
            default:
                // default color is white
                colored.insert(0, "\033[1;37m");
                break;
            }
            colored.append("\033[0m");
        }

        friend std::ostream &operator<<(std::ostream &, const ColorizedString &);
        
        std::string colored;
    };
}
#endif
