# Contributing

This repository is the C++ daemon client for Kryptic (`kryptic-daemon-client`).

## What we accept

- Bug fixes and test coverage
- Documentation corrections
- Compatibility fixes

## What we do not accept

- Storing or logging secret values in plaintext
- Breaking changes to the daemon IPC protocol without a coordinated Daemon release

- Public GitHub issues for vulnerabilities (email security@kryptic.dev)

## Development

```bash
cmake --build build && ctest --test-dir build
```

Protocol details: [Kryptic.Daemon/PROTOCOL.md](https://github.com/dev-kryptic/Kryptic.Daemon/blob/main/PROTOCOL.md).

## Releasing

A merge to `main` is the release. The publish workflow commits the version bump as
the Kryptic Release Bot, publishes to GitHub Releases (source tags), tags `vX.Y.Z`, and opens a
GitHub Release using the matching section in [CHANGELOG.md](CHANGELOG.md).

Leave release-worthy notes under **Unreleased**. The publish workflow creates
the `## X.Y.Z` section from that body if it is missing (or a one-line fallback)
and commits it.

## Licensing of contributions

This repository is Apache-2.0. By opening a pull request you confirm the
contribution is your own work (or you have the right to submit it) and you
license it under Apache-2.0. There is no CLA.
