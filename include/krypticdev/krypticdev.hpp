#pragma once

#include <string>

namespace krypticdev {

// What inject() did.
struct Result {
    int injected = 0;
    bool skipped = false;
    std::string reason;
};

// Optional overrides. Empty strings and timeout_ms == 0 mean "use the default"
// (environment variables, then kryptic.json, then built-in defaults).
struct Options {
    std::string environment;
    std::string project_id;
    int timeout_ms = 0;
};

// Fetches secrets from the local Kryptic daemon and sets them with setenv /
// _putenv_s. Existing environment variables are never overwritten.
//
// Outside development (CPP_ENV / APP_ENV / ENVIRONMENT / ENV = production or
// staging, or KRYPTIC_DISABLED=true) this is a no-op. It never throws: a
// missing daemon means the application starts with the environment it already
// has.
Result inject();
Result inject(const Options& options);

}  // namespace krypticdev
