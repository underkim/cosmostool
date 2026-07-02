#include "ShellQuote.h"

namespace OpenC3::Core::Connection {

std::string shellQuote(const std::string& value)
{
    std::string out;
    // Worst case every char is a single quote expanding to 4 chars, plus the
    // surrounding quotes. Reserve generously to avoid repeated reallocation.
    out.reserve(value.size() + 2);

    out.push_back('\'');
    for (const char c : value) {
        if (c == '\'') {
            // Close the quote, emit an escaped quote, reopen: ' -> '\''
            out += "'\\''";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('\'');

    return out;
}

bool contentEndsHeredoc(const std::string& content, const std::string& delimiter)
{
    if (delimiter.empty())
        return false;
    if (content == delimiter)                       // whole payload is the delimiter
        return true;
    if (content.rfind(delimiter + "\n", 0) == 0)    // first line
        return true;
    const std::string tail = "\n" + delimiter;
    if (content.size() >= tail.size() &&            // last line
        content.compare(content.size() - tail.size(), tail.size(), tail) == 0)
        return true;
    return content.find("\n" + delimiter + "\n") != std::string::npos; // middle line
}

} // namespace OpenC3::Core::Connection
