# Coding Standards

This document defines the coding standards for the `yamlget` codebase. All contributors are expected to follow these rules. Deviations require a comment explaining the reason.

## Language standard

- **C99** (`-std=c99`). No compiler extensions.
- Do not use C11, C17, or later features unless guarded with a `#if __STDC_VERSION__` check that provides a C99 fallback.
- Do not use `__attribute__`, `__declspec`, or any GCC/Clang/MSVC-specific syntax in portable code. Isolate platform-specific code in dedicated translation units or behind `#ifdef` guards.

## File layout

```
include/        Public headers only. No implementation in headers.
src/            Implementation files. One logical unit per file.
tests/          Test scripts and fixtures. No production code here.
docs/           Documentation.
```

Every `.c` file must include its own `.h` counterpart first (where one exists), before any system headers. This catches missing self-contained declarations.

```c
#include "yamlget/lexer.h"   /* own header first */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
```

## Naming conventions

| Category | Convention | Example |
|----------|------------|---------|
| Functions | `snake_case` | `yaml_lexer_next_token` |
| Types (`typedef struct`) | `snake_case_t` | `yaml_token_t` |
| Enum values | `UPPER_SNAKE_CASE` | `YAML_TOKEN_SCALAR` |
| Macros / constants | `UPPER_SNAKE_CASE` | `YAMLGET_VERSION` |
| File-local (`static`) | `snake_case` (no prefix) | `skip_whitespace` |
| Public API | Prefixed with `yamlget_` | `yamlget_lookup` |

Do not use single-letter names except for loop indices (`i`, `j`, `k`) and trivial temporaries.

## Formatting

- **Indent:** 4 spaces. No tabs.
- **Line length:** 100 characters maximum. Wrap with a trailing `\` or restructure.
- **Braces:** K&R style (opening brace on the same line).

```c
if (condition) {
    do_something();
} else {
    do_other();
}
```

- **Spaces:** one space after keywords (`if`, `for`, `while`, `return`), no space between a function name and `(`.
- **Pointer placement:** `type *name`, not `type* name`.

## Error handling

- Functions that can fail return an `int` error code or a typed enum. Never return `-1`; use a named constant.
- The caller is always responsible for checking the return value. Do not silently swallow errors.
- All error messages go to `stderr`. Use `fprintf(stderr, ...)`, never `printf` for errors.
- Error messages must be lowercase, end without a period, and include the program name:

  ```
  yamlget: file not found: 'config.yaml'
  yamlget: key not found: 'database.port'
  ```

## Memory management

- Prefer stack allocation. Avoid `malloc` for small, bounded data.
- Every `malloc` must have a paired `free` on every exit path.
- Do not use `realloc` without checking the return value.
- No memory leaks on any exit path, including error paths. Verify with ASan (`make asan`).
- Do not use `alloca`: it is not portable and its failure is silent.

## Input handling

- Never assume input is null-terminated unless you control the source.
- Always validate buffer bounds before writing. Use `snprintf`, not `sprintf`.
- Never use `gets`. Use `fgets` with an explicit size.
- Treat all input (file paths, YAML content, key paths) as untrusted.

## Comments

Write comments only when the **why** is non-obvious. Do not narrate what the code does.

```c
/* Bad: narrates the obvious */
i++;  /* increment i */

/* Good: explains a non-obvious constraint */
/* YAML spec §6.5: a block scalar's chomping indicator must follow the
   indentation indicator, not precede it. */
```

Use `/* */` for all comments (C99 block style). `//` line comments are permitted but not preferred.

## Portability

- Use `<stdint.h>` types (`uint8_t`, `size_t`, `int32_t`) where fixed width matters.
- Do not assume `int` or `long` width.
- Do not use POSIX-only functions in the parser or lookup core. Isolate POSIX calls (e.g., `fileno`, `isatty`) in a platform shim.
- `size_t` for lengths and indices; `ptrdiff_t` for pointer differences.
- Avoid variable-length arrays (VLAs): MSVC does not support them.

## Exit codes

All exit codes are defined in `include/yamlget.h`. Never call `exit()` with a bare integer literal.

```c
/* Bad */
exit(1);

/* Good */
exit(YAMLGET_EXIT_NOT_FOUND);
```

## Testing

- Every public function must have at least one test fixture.
- Tests are shell scripts in `tests/` that invoke the built binary against files in `tests/fixtures/`.
- A test must check the exit code **and** the output content.
- Add a fixture for any YAML construct you parse; do not test against inline strings only.

## CI requirements

Every pull request must pass:

- Build with `gcc -Werror` on Linux
- Build with `clang -Werror` on Linux
- Build with `clang -Werror` on macOS
- Build with MSVC `/WX` on Windows
- All tests via `make test`
- ASan + UBSan clean (`make asan && make test`)
