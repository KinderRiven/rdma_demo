#include "coloring.hpp"

namespace Hill {
    std::ostream &operator<<(std::ostream &s, const ColorizedString &cs) {
        s << cs.colored;
        return s;
    }
}
