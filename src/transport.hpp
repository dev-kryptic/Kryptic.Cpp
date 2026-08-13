#pragma once

#include <chrono>
#include <string>

namespace krypticdev {

std::string socket_path();

// Writes one NDJSON request line to the daemon and returns the one response
// line (without the trailing newline). Throws std::runtime_error on failure.
std::string round_trip(const std::string& line, std::chrono::milliseconds timeout);

}  // namespace krypticdev
