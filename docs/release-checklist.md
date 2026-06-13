# Release Checklist

Use this checklist to cut a `yamlget` release. Work top-to-bottom. Do not tag
until every item is ticked.

---

## 1. Local build

```sh
make clean
make CC=gcc   CFLAGS="-std=c99 -Wall -Wextra -Wpedantic -O2 -Werror"
make CC=clang CFLAGS="-std=c99 -Wall -Wextra -Wpedantic -O2 -Werror"
```

- [ ] Zero warnings on both gcc and clang
- [ ] `./yamlget --version` prints the expected version string
- [ ] `./yamlget --help` exits 0 and prints usage to stdout (not stderr)

---

## 2. Full test suite

```sh
make test
```

- [ ] All lexer unit tests pass
- [ ] All integration tests pass
- [ ] Zero failures reported

---

## 3. Sanitizer tests

```sh
make asan-test CC=clang \
  CFLAGS="-std=c99 -Wall -Wextra -g -fsanitize=address,undefined -O1"
```

- [ ] Zero AddressSanitizer errors
- [ ] Zero UndefinedBehaviorSanitizer errors

---

## 4. Binary size

```sh
strip yamlget
du -sh yamlget
```

- [ ] Binary is under 200 KB (stripped) on Linux x86_64

---

## 5. Benchmark

```sh
make bench BENCH_N=200
```

- [ ] Benchmark completes without crashes
- [ ] yamlget result matches the expected value (`benchmark_http_requests_total`)
- [ ] Numbers are in the expected range (< 5 ms/op on typical hardware)

---

## 6. Documentation

- [ ] `CHANGELOG.md` has a complete entry for this version (not `[Unreleased]`)
- [ ] `README.md` status table shows the release as complete
- [ ] `docs/roadmap-v0.1.0.md` all M5 items are ticked
- [ ] `include/yamlget.h` version constants match the tag (`MAJOR`, `MINOR`, `PATCH`, `STRING`)

---

## 7. CI verification

Push the release branch (not the tag yet) to GitHub and verify:

- [ ] `linux-gcc` job passes
- [ ] `linux-clang` job passes
- [ ] `linux-gcc-asan` job passes
- [ ] `macos-clang` job passes
- [ ] `windows-msvc` job passes
- [ ] `windows-mingw` job passes

---

## 8. Tag the release

```sh
# Verify the version string one more time before tagging
./yamlget --version

# Create an annotated tag (preferred) or signed tag (if you have a GPG key)
git tag -a v0.1.0 -m "Release v0.1.0"
# or: git tag -s v0.1.0 -m "Release v0.1.0"

# Push the tag - this triggers the release workflow
git push origin v0.1.0
```

- [ ] Tag is pushed to GitHub
- [ ] Release workflow (`.github/workflows/release.yml`) is triggered

---

## 9. Release workflow validation

Wait for the release workflow to complete, then verify:

- [ ] `build-linux-x86_64` job passed
- [ ] `build-macos-x86_64` job passed
- [ ] `build-macos-arm64` job passed
- [ ] `build-windows-x86_64` job passed
- [ ] `release` (publish) job passed

---

## 10. Artifact smoke tests

Download each artifact from the GitHub Release page and run:

```sh
# Linux (on a Linux machine or container)
chmod +x yamlget-linux-x86_64
./yamlget-linux-x86_64 --version       # must print 0.1.0
echo "key: value" | ./yamlget-linux-x86_64 - key   # must print "value"

# macOS Intel (on an Intel Mac or Rosetta)
chmod +x yamlget-macos-x86_64
./yamlget-macos-x86_64 --version

# macOS Apple Silicon
chmod +x yamlget-macos-arm64
./yamlget-macos-arm64 --version

# Windows PowerShell
.\yamlget-windows-x86_64.exe --version
```

- [ ] Linux binary: `--version` exits 0, prints correct version
- [ ] macOS x86_64 binary: runs correctly
- [ ] macOS arm64 binary: runs correctly
- [ ] Windows binary: runs correctly

---

## 11. GitHub Release page

- [ ] Release is titled correctly (`yamlget v0.1.0`)
- [ ] Release is marked as "Latest release" (not pre-release, not draft)
- [ ] All 4 binaries are attached as assets
- [ ] Release notes are accurate

---

## 12. Post-release

```sh
# Open the next development cycle
git checkout -b next-dev
```

- [ ] Add `## [Unreleased]` section to `CHANGELOG.md`
- [ ] Open GitHub milestone for v0.2.0 (if applicable)
- [ ] Close GitHub milestone for v0.1.0

---

## Common failure modes

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| Release workflow does not trigger | Tag not pushed, or tag format wrong | Tag must match `v[0-9]*.[0-9]*.[0-9]*` |
| Artifact missing from release | Job failed or artifact name mismatch | Check job logs; fix artifact path in release.yml |
| Binary fails on target platform | Cross-compile issue or missing static link | Build natively; check `ldd` output on Linux |
| Version mismatch in binary | `YAMLGET_VERSION_STRING` not updated | Update `include/yamlget.h` before tagging |
