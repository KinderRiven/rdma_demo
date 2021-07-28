#ifndef __HILL__CMDPARSER__
#define __HILL__CMDPARSER__

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <regex>
#include <sstream>
#include <optional>

namespace CmdParser {
    namespace CmdParserValues {
        class BasicValue : public std::enable_shared_from_this<BasicValue> {
        public:
            BasicValue() = default;
            virtual ~BasicValue() = default;
        };

        template <typename T>
        class GenericValue : public BasicValue {
        public:
            GenericValue() = default;
            GenericValue(const GenericValue &) = default;
            GenericValue(GenericValue &&) = default;
            GenericValue(const T& t) : content(t) {};
            GenericValue(T &&t) : content(t) {};
            ~GenericValue() = default;

            auto operator=(const GenericValue<T> &) -> GenericValue<T>& = default;
            auto operator=(GenericValue<T>&&) -> GenericValue<T>& = default;
            auto operator=(const T& t) -> GenericValue<T>& {
                content = t;
                return *this;
            }
            auto operator=(T&& t) -> GenericValue<T>& {
                content = t;
                return *this;
            }

            auto unwrap() noexcept -> T& {
                return content;
            }

            auto unwrap_const() const noexcept -> const T& {
                return content;
            }

        private:
            T content;
        };
    }

    class Parser {
    public:
        using BasicValuePtr = std::shared_ptr<CmdParserValues::BasicValue>;
        using OptionMap = std::unordered_map<std::string, std::string>;
        using RegexMap = std::unordered_map<std::string, std::pair<std::string, std::string>>;
        using ParsedOptionMap = std::unordered_map<std::string, BasicValuePtr>;

        Parser() = default;
        Parser(const Parser &) =default;
        Parser(Parser &&) =default;
        auto operator=(const Parser &) -> Parser & = default;
        auto operator=( Parser &&) -> Parser & = default;
        ~Parser() = default;

        auto add_switch(const std::string &full, const std::string &shrt) -> bool {
            return regex_map.insert({full, {full + regex_long_suffix, shrt + regex_short_suffix}}).second;
        }

        auto add_switch(const std::string &full, const std::string &shrt, bool default_value) -> bool {
            auto pair = std::make_pair(full + regex_long_suffix, shrt + regex_short_suffix);
            if (!add_switch(full, shrt)) {
                return false;
            }
            return plain_map.insert({full, default_value ? "true" : "false"}).second;
        }

        auto add_option(const std::string &full, const std::string &shrt) -> bool {
            return regex_map.insert({full, {full + regex_long_suffix, shrt + regex_short_suffix}}).second;
        }

        template<typename T>
        auto add_option(const std::string &full, const std::string &shrt, const T& default_value) -> bool {
            auto pair = std::make_pair(full + regex_long_suffix, shrt + regex_short_suffix);
            if (!add_option(full, shrt)) {
                return false;
            }

            if constexpr (std::is_floating_point_v<T>) {
                return plain_map.insert({full, std::to_string(default_value)}).second;
            } else {
                std::stringstream stream;
                stream << default_value;
                return plain_map.insert({full, stream.str()}).second;
            }
            return false;
        }

        auto parse(int argc, char *argv[]) -> void {
            std::stringstream buf;
            for (int i = 1; i < argc; i++) {
                buf << argv[i] << " ";
            }
            auto str = buf.str();

            for (const auto &reg : regex_map) {
                std::regex lregex(reg.second.first);
                std::regex sregex(reg.second.second);
                std::smatch match;
                if (std::regex_search(str, match, lregex) || std::regex_search(str, match, sregex)) {
                    if (auto ret = plain_map.find(reg.first); ret == plain_map.end()) {
                        plain_map.insert({reg.first, match[1]});
                    } else {
                        ret->second = match[1];
                    }
                }
            }
        }

        template<typename Target>
        auto parse_value(const std::string &in) -> std::optional<Target> {
            Target holder;
            std::stringstream buf(in);
            buf >> holder;
            if (buf.fail()) {
                return {};
            }
            return holder;
        }


        auto get_plain(const std::string &opt) const noexcept -> std::optional<std::string> {
            if (auto ret = plain_map.find(opt); ret != plain_map.end()) {
                return {ret->second};
            }
            return {};
        }

        template<typename T,
                 typename = typename std::enable_if_t<std::is_same_v<T, bool>>>
        auto get_as(const std::string &opt) -> std::optional<bool> {
            auto maybe_parsed = parsed_map.find(opt);

            // find a parsed value
            if (maybe_parsed != parsed_map.end()) {
                auto tmp = std::dynamic_pointer_cast<CmdParserValues::GenericValue<bool>>(maybe_parsed->second);
                return tmp->unwrap();
            }

            // not parsed, thus parse it
            bool parsed = true;
            if (auto plain = plain_map.find(opt); plain != plain_map.end()) {
                std::regex false_regex("(f|F)(alse)?|0");
                if (std::regex_match(plain->second, false_regex)) {
                    std::cout << ">> matched\n";
                    parsed = false;
                }
            } else {
                return {};
            }

            // parsed, add this to the parsed_map
            auto value = std::make_shared<CmdParserValues::GenericValue<bool>>(parsed);
            BasicValuePtr ptr = value->shared_from_this();
            parsed_map.insert({opt, ptr});

            return parsed;
        }


        template<typename Target,
                 typename = typename std::enable_if_t<!std::is_same_v<Target, bool>>>
        auto get_as(const std::string &opt) -> std::optional<Target> {
            auto maybe_parsed = parsed_map.find(opt);

            // find a parsed value
            if (maybe_parsed != parsed_map.end()) {
                auto tmp = std::dynamic_pointer_cast<CmdParserValues::GenericValue<Target>>(maybe_parsed->second);
                return tmp->unwrap();
            }

            auto plain = plain_map.find(opt);
            if (plain == plain_map.end()) {
                // switch not supplied
                return {};
            }

            if (auto r = parse_value<Target>(plain->second); r.has_value()) {
                auto parsed = r.value();
                auto value = std::make_shared<CmdParserValues::GenericValue<Target>>(parsed);
                BasicValuePtr ptr = value->shared_from_this();
                parsed_map.insert({opt, ptr});
                return parsed;
            } else {
                return {};
            }
        }

        template<typename Target>
        auto get_as(const std::string &opt,
                    std::function<std::optional<Target>(const std::string &s)> convertor) -> std::optional<Target> {

            auto plain = plain_map.find(opt);
            if (plain == plain_map.end()) {
                // switch not supplied
                return {};
            }
            
            return convertor(plain->second);
        }

        auto dump_regex() const noexcept -> void {
            for (const auto &p : regex_map) {
                std::cout << ">> Option: " << p.first << "\n";
                std::cout << "   Regex:  " << p.second.first << "\n";
                std::cout << "   Regex:  " << p.second.second << "\n";
            }
        }

        auto dump_plain() const noexcept -> void {
            for (const auto &p : plain_map) {
                std::cout << ">> Option: " << p.first << "\n";
                std::cout << "   Value:  " << get_plain(p.first).value() << "\n";
            }
        }

    private:
        static const std::string regex_long_suffix;
        static const std::string regex_short_suffix;
        OptionMap plain_map;
        RegexMap regex_map;
        ParsedOptionMap parsed_map;
    };

}
#endif
