# Chromium CitizenNotes docs

This directory contains [Chrome CitizenNotes] project
documentation in [Gitiles-flavored Markdown]. It is automatically
[rendered by Gitiles].

[Chrome CitizenNotes]: https://developer.chrome.com/docs/citizennotes/
[Gitiles-flavored Markdown]: https://gerrit.googlesource.com/gitiles/+/master/Documentation/markdown.md
[rendered by Gitiles]: https://chromium.googlesource.com/citizennotes/citizennotes-frontend/+/main/docs/

**If you add new documents, please also add a link to them in the [Document Index](#Document-Index)
below.**

[TOC]

## Document Index

### Design Documents
*   See the shared [Design Documents](https://drive.google.com/drive/folders/1JbUthATfybvMQR3yAHC4J0P7n6oftYNq) folder in the Chromium drive.

### General Development
*   [Get the Code](get_the_code.md)
*   [Contributing Changes](contributing_changes.md)
*   [Chrome CitizenNotes Design Review Guidelines](design_guidelines.md)
*   [Release Management](release_management.md)
*   [Dependencies](dependencies.md)
*   [Localization](l10n.md)
*   [Material 3 in CitizenNotes](material3_guidelines.md)
*   [V8 debugger support checklist for new language features](https://goo.gle/v8-checklist)
*   [Chrome CitizenNotes Protocol](citizennotes-protocol.md)
*   [Visual logging in CitizenNotes](visual_logging.md)
*   [UMA metrics in CitizenNotes](uma_metrics.md)
    *   [How to add UMA metrics in CitizenNotes frontend](add_uma_metrics.md)
*   [How to add experiments in CitizenNotes frontend](add_experiments.md)

### Testing
*   [Testing Chromium CitizenNotes](testing.md)
*   [E2E test guide](../test/e2e/README.md)
*   [Unit test guide](../test/unittests/README.md)

### Architectural Documentation
*   [Architecture of CitizenNotes](architecture_of_citizennotes.md)
*   [Resource management in CitizenNotes](resource_management.md)

### Chromium
*   [Chromium Docs](https://chromium.googlesource.com/chromium/src/+/master/docs/README.md)
*   [V8 Documention](https://v8.dev/docs)

### Useful Commands

`git cl format --js`

Formats all code using clang-format.

`npm run check`

Runs all static analysis checks on CitizenNotes code.


## Creating Documentation

### Guidelines

*   See the [Chromium Documentation Guidelines](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/documentation_guidelines.md)
    and the
    [Chromium Documentation Best Practices](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/documentation_best_practices.md).
*   Markdown documents must follow the
    [style guide](https://github.com/google/styleguide/tree/gh-pages/docguide).

### Previewing changes

#### Locally using [md_browser](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/tools/md_browser)

Assuming that `/path/to/src` contains a chromium checkout, you can run:

```bash
# in citizennotes-frontend checkout
/path/to/src/tools/md_browser/md_browser.py --directory $PWD
```

and preview the result by opening http://localhost:8080/docs/README.md in your browser. This is only an estimate. The **gitiles** view may differ.

#### Online with Gerrit's links to gitiles

1.  Upload a patch to gerrit, or receive a review request.
    e.g. https://chromium-review.googlesource.com/c/3362532
2.  View a specific .md file.
    e.g. https://chromium-review.googlesource.com/c/3362532/2/docs/README.md
3.  You will see something like <br>
    Base
    [preview](https://chromium.googlesource.com/chromium/src/+/ad44f6081ccc6b92479b12f1eb7e9482f474859d/docs/README.md)
    -> Patchset 3
    [preview](https://chromium.googlesource.com/chromium/src/+/refs/changes/32/3362532/3/docs/README.md)
    | DOWNLOAD <br>
    at the top left of the page. Click on the second
    "[preview](https://chromium.googlesource.com/chromium/src/+/refs/changes/32/3362532/3/docs/README.md)"
    link to open the preview for the current patch set.

This **gitiles** view is the authoritative view, exactly the same as will be
used when committed.
