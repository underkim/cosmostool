#pragma once

#include <string>

namespace OpenC3::Core::Connection {

/// Quote a string for safe use as a single argument in a POSIX shell command.
///
/// All commands are run through a POSIX shell (WSL/SSH -> bash), so values that
/// are interpolated into command strings (paths, container names, plugin names,
/// any user-provided value) must be quoted to avoid word-splitting and command
/// injection.
///
/// The value is wrapped in single quotes; embedded single quotes are escaped
/// using the standard `'\''` idiom. The result is always quoted, even for
/// simple values, so the output is predictable.
///
/// Examples:
///   hello        -> 'hello'
///   hello world  -> 'hello world'
///   a'b          -> 'a'\''b'
///   $(rm -rf /)  -> '$(rm -rf /)'
[[nodiscard]] std::string shellQuote(const std::string& value);

/// Return true if `content` contains a line exactly equal to `delimiter`.
///
/// Writing a file with `cat > path << 'DELIM' ... DELIM` is safe from expansion
/// (the delimiter is quoted), but a line in the payload equal to DELIM would
/// close the heredoc early and let the following lines run as shell commands.
/// Callers use this to reject such content and fail closed.
[[nodiscard]] bool contentEndsHeredoc(const std::string& content,
                                      const std::string& delimiter);

/// RFC 4648 base64 encode, wrapped at 76 columns (the width the `base64` /
/// `base64 -d` coreutils and most other implementations expect for MIME-style
/// input).
///
/// Used to move arbitrary (including binary) content over a remote exec
/// channel that only accepts text: the output alphabet is pure ASCII, so it
/// cannot contain shell metacharacters or a line matching a heredoc delimiter,
/// unlike the raw bytes it encodes.
[[nodiscard]] std::string base64Encode(const std::string& data);

} // namespace OpenC3::Core::Connection
