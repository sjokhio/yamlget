# AI Usage

`yamlget` is developed with AI coding assistants as one tool among many. This document describes how they are used and what that means for contributors and users.

## How AI assistance is used

AI assistants (such as Claude) are used during development to:

- Draft and iterate on implementation code
- Suggest test cases and fixture structures
- Review documentation for clarity and consistency
- Explore approaches to design problems

## What the human maintainer does

All changes to this repository are reviewed, tested, and accepted by a human maintainer before they land. Specifically:

- Every commit is read and understood by a human before being staged.
- All code changes are compiled and run locally against the test suite.
- CI must pass before a branch is considered mergeable.
- AI-generated suggestions are treated as draft material, not authoritative output. They are accepted, modified, or rejected based on independent judgement.

## What this means for correctness

AI assistants can produce plausible-looking code that is subtly wrong. The safeguards in this project against that are:

- A test suite (`make test`) that covers expected behavior and known edge cases.
- Sanitizer testing (`make asan-test`) to catch memory and undefined-behavior errors.
- CI running across Linux (gcc, clang), macOS, and Windows on every push.
- Manual review of all non-trivial logic changes before commit.

No AI output is trusted on the basis of confidence alone.

## For contributors

If you used an AI assistant substantially while preparing a contribution, please mention it briefly in your pull request description. There is no policy against AI-assisted contributions; the disclosure helps reviewers calibrate how much independent verification to apply.

"Substantially" means: the AI wrote most of the logic, not just auto-completed a few lines or reformatted code.

You do not need to disclose minor assistance such as asking an AI to explain a function, generate a one-liner, or check spelling.
