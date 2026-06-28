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

} // namespace OpenC3::Core::Connection
