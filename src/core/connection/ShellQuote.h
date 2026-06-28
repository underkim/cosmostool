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

} // namespace OpenC3::Core::Connection
