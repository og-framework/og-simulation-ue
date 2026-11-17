<!-- SPDX-License-Identifier: MPL-2.0 -->
# Contributing to OG Sim Net

## License

This project is licensed under the Mozilla Public License 2.0. By contributing you agree that your contributions are licensed under the same terms — inbound = outbound MPL 2.0. You retain copyright ownership of your contributions.

DCO sign-off (`Signed-off-by:` line in commit messages) is appreciated but not enforced.

## Pull requests

- Open an issue first for non-trivial changes so we can align on approach before you invest time writing code.
- Keep PRs focused: one logical change per PR.
- Ensure the tests under `tests/` pass before submitting. There is no standalone build yet: they build through the host project's low-level-test target.
- Add `// SPDX-License-Identifier: MPL-2.0` to any new source files you author.

## Issues

Please include a minimal reproduction and the platform/compiler version when reporting bugs.
