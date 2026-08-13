# krypticdev (C++ SDK)

Zero-dependency C++17 client. During development startup, `krypticdev::inject()`
asks the local Kryptic daemon for the current project's secrets and sets them as
environment variables. Outside development it is a no-op. It never throws: a
missing daemon means the application simply starts with the environment it
already has.

```cmake
FetchContent_Declare(
  krypticdev
  GIT_REPOSITORY https://github.com/dev-kryptic/krypticdev-cpp.git
  GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(krypticdev)
target_link_libraries(your_app PRIVATE krypticdev)
```

Or as a subdirectory:

```cmake
add_subdirectory(path/to/krypticdev-cpp)
target_link_libraries(your_app PRIVATE krypticdev)
```

```cpp
#include <krypticdev/krypticdev.hpp>
#include <cstdlib>

int main() {
    krypticdev::inject();  // populates the process environment in development

    const char* db_url = std::getenv("DATABASE_URL");
}
```

No-op outside development (`CPP_ENV`/`APP_ENV`/`ENVIRONMENT`/`ENV` = production/staging, or
`KRYPTIC_DISABLED=true`). Finds `kryptic.json` by walking up from the working directory,
never overwrites existing environment variables, never throws. Configuration:
`KRYPTIC_PROJECT_ID`, `KRYPTIC_ENV`, `KRYPTIC_SOCKET_PATH`, `KRYPTIC_TIMEOUT_MS`,
`KRYPTIC_DISABLED`, `KRYPTIC_SILENT`.

Protocol: [daemon/PROTOCOL.md](https://github.com/dev-kryptic/Kryptic.Daemon/blob/main/PROTOCOL.md). License: Apache-2.0.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Without CMake:

```bash
c++ -std=c++17 -Iinclude -Isrc src/json.cpp src/krypticdev.cpp src/transport_unix.cpp \
  tests/kryptic_test.cpp -o krypticdev_tests
./krypticdev_tests
```
