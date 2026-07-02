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

std::string base64Encode(const std::string& data)
{
    static constexpr char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string packed;
    packed.reserve(((data.size() + 2) / 3) * 4);

    std::size_t i = 0;
    for (; i + 3 <= data.size(); i += 3) {
        const auto b0 = static_cast<unsigned char>(data[i]);
        const auto b1 = static_cast<unsigned char>(data[i + 1]);
        const auto b2 = static_cast<unsigned char>(data[i + 2]);
        packed += kTable[b0 >> 2];
        packed += kTable[((b0 & 0x03) << 4) | (b1 >> 4)];
        packed += kTable[((b1 & 0x0F) << 2) | (b2 >> 6)];
        packed += kTable[b2 & 0x3F];
    }

    const std::size_t remaining = data.size() - i;
    if (remaining == 1) {
        const auto b0 = static_cast<unsigned char>(data[i]);
        packed += kTable[b0 >> 2];
        packed += kTable[(b0 & 0x03) << 4];
        packed += "==";
    } else if (remaining == 2) {
        const auto b0 = static_cast<unsigned char>(data[i]);
        const auto b1 = static_cast<unsigned char>(data[i + 1]);
        packed += kTable[b0 >> 2];
        packed += kTable[((b0 & 0x03) << 4) | (b1 >> 4)];
        packed += kTable[(b1 & 0x0F) << 2];
        packed += "=";
    }

    // Wrap so a large file doesn't become one unbounded line.
    std::string wrapped;
    wrapped.reserve(packed.size() + packed.size() / 76 + 1);
    for (std::size_t pos = 0; pos < packed.size(); pos += 76) {
        wrapped.append(packed, pos, 76);
        wrapped += '\n';
    }
    return wrapped;
}

} // namespace OpenC3::Core::Connection
