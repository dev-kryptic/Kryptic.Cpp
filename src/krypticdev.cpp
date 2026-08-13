#include <krypticdev/krypticdev.hpp>

#include "json.hpp"
#include "transport.hpp"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <stdlib.h>
#endif

namespace krypticdev {
namespace {

constexpr int kProtocolVersion = 1;
constexpr int kDefaultTimeoutMs = 2000;

std::string env(const char* name) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string{};
}

bool env_exists(const std::string& name) {
#ifdef _WIN32
    std::size_t needed = 0;
    getenv_s(&needed, nullptr, 0, name.c_str());
    return needed > 0;
#else
    return std::getenv(name.c_str()) != nullptr;
#endif
}

void set_env(const std::string& name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    ::setenv(name.c_str(), value.c_str(), 1);
#endif
}

std::string to_lower(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

void warn(const std::string& message) {
    if (env("KRYPTIC_SILENT") == "true") return;
    std::cerr << "[kryptic] " << message << '\n';
}

std::string should_skip() {
    if (env("KRYPTIC_DISABLED") == "true") return "disabled";

    // C++ has no single convention; honor the common ones.
    const char* variables[] = {"CPP_ENV", "APP_ENV", "ENVIRONMENT", "ENV"};
    for (const char* variable : variables) {
        const std::string value = to_lower(env(variable));
        if (value == "production" || value == "prod" || value == "staging") {
            return to_lower(variable) + "_" + value;
        }
    }
    return {};
}

int timeout_ms(const Options& options) {
    if (options.timeout_ms > 0) return options.timeout_ms;
    const std::string raw = env("KRYPTIC_TIMEOUT_MS");
    if (!raw.empty()) {
        try {
            return std::stoi(raw);
        } catch (...) {
            // fall through to the default
        }
    }
    return kDefaultTimeoutMs;
}

json::Value find_kryptic_json() {
    std::error_code ec;
    std::filesystem::path directory = std::filesystem::current_path(ec);
    if (ec) return {};

    while (true) {
        const std::filesystem::path candidate = directory / "kryptic.json";
        if (std::filesystem::is_regular_file(candidate, ec)) {
            std::ifstream in(candidate);
            if (!in) {
                warn("could not read " + candidate.string() + " - ignoring it.");
                return {};
            }
            std::ostringstream buffer;
            buffer << in.rdbuf();
            try {
                return json::parse(buffer.str());
            } catch (...) {
                warn("could not parse " + candidate.string() + " - ignoring it.");
                return {};
            }
        }
        const std::filesystem::path parent = directory.parent_path();
        if (parent == directory) return {};
        directory = parent;
    }
}

}  // namespace

Result inject() {
    return inject(Options{});
}

Result inject(const Options& options) {
    try {
        const std::string skip_reason = should_skip();
        if (!skip_reason.empty()) {
            return Result{0, true, skip_reason};
        }

        const json::Value config = find_kryptic_json();

        std::string project_id = options.project_id;
        if (project_id.empty()) project_id = env("KRYPTIC_PROJECT_ID");
        if (project_id.empty()) {
            if (const json::Value* value = config.get("projectId")) {
                project_id = value->as_string();
            }
        }
        if (project_id.empty()) {
            warn("no kryptic.json found (and no KRYPTIC_PROJECT_ID set) - nothing to inject.");
            return Result{0, true, "no_project"};
        }

        std::string environment = options.environment;
        if (environment.empty()) environment = env("KRYPTIC_ENV");
        if (environment.empty()) {
            if (const json::Value* value = config.get("defaultEnvironment")) {
                environment = value->as_string();
            }
        }
        if (environment.empty()) environment = "development";

        const std::string request =
            std::string("{\"v\":") + std::to_string(kProtocolVersion) +
            ",\"type\":\"secrets\",\"projectId\":" + json::quote(project_id) +
            ",\"environment\":" + json::quote(environment) + "}";

        std::string line;
        try {
            line = round_trip(request, std::chrono::milliseconds(timeout_ms(options)));
        } catch (const std::exception& e) {
            warn(std::string("daemon not reachable (") + e.what() + ") - continuing without injected secrets.");
            return Result{0, true, "daemon_unreachable"};
        }

        json::Value response;
        try {
            response = json::parse(line);
        } catch (...) {
            warn("daemon sent an invalid response - continuing without injected secrets.");
            return Result{0, true, "invalid_response"};
        }

        if (!response.get("ok") || !response.get("ok")->as_bool()) {
            std::string error = "internal";
            if (const json::Value* value = response.get("error")) {
                if (!value->as_string().empty()) error = value->as_string();
            }
            std::string message;
            if (const json::Value* value = response.get("message")) {
                message = value->as_string();
            }
            warn("daemon refused the request (" + error + "): " + message);
            return Result{0, true, error};
        }

        int injected = 0;
        const json::Value* secrets = response.get("secrets");
        if (secrets && secrets->type == json::Value::Type::Array) {
            for (const json::Value& entry : secrets->array) {
                if (entry.type != json::Value::Type::Object) continue;
                const json::Value* key = entry.get("key");
                const json::Value* value = entry.get("value");
                if (!key || key->as_string().empty()) continue;
                if (env_exists(key->string)) continue;  // real environment always wins
                set_env(key->string, value ? value->as_string() : std::string{});
                ++injected;
            }
        }

        return Result{injected, false, {}};
    } catch (...) {
        warn("unexpected error - continuing without injected secrets.");
        return Result{0, true, "internal"};
    }
}

}  // namespace krypticdev
