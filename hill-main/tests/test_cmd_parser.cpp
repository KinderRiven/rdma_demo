#include "cmd_parser/cmd_parser.hpp"

#include <sstream>
using namespace CmdParser;

template<typename T>
auto show(Parser &parser, const std::string &opt) -> void {
    auto ret = parser.get_as<T>(opt);
    if (ret.has_value()) {
        std::cout << "value for " << opt << " is " << ret.value() << "\n";
    } else {
        std::cout << opt << " is not supplied\n";
    }
}

static const char *argv[] = {
    "program",
    "--option1=4321",
    "-o2 -187654321",
    "--option3=8",
    "-o4 this_is_a_new_string",
};

auto main() -> int{
    Parser parser;
    parser.add_option<int>("--option1", "-o1", 1234);
    parser.add_option<long>("--option2", "-o2", 12345678L);
    parser.add_option<uint64_t>("--option3", "-o3", 1234567891234ULL);
    parser.add_option<std::string>("--option4", "-o4", "a_string");
    parser.add_option<char>("--option5", "-o5", 'a');
    parser.add_option<float>("--option6", "-o6", 123.4);
    parser.add_option<double>("--option7", "-o7", 1234.4321f);
    parser.add_option<long double>("--option8", "-o8", 12345.54321F);
    
    parser.parse(5, (char **)argv);
    parser.dump_plain();

    auto o7 = parser.get_as<double>("--option7").value();
    auto o2 = parser.get_as<long>("--option2").value();
    auto o5 = parser.get_as<char>("--option5").value();
    auto o4 = parser.get_as<std::string>("--option4").value();

    std::cout << o7 << "\n";
    std::cout << o2 << "\n";
    std::cout << o5 << "\n";
    std::cout << o4 << "\n";    
}
