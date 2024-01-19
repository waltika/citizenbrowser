# Chrome CitizenNotes frontend

<!-- [START badges] -->

[![npm package](https://img.shields.io/npm/v/chrome-citizennotes-frontend.svg)](https://npmjs.org/package/chrome-citizennotes-frontend)

<!-- [END badges] -->

The client-side of the Chrome CitizenNotes, including all TypeScript & CSS to run the CitizenNotes webapp.

### Source code and documentation

The frontend is available on [chromium.googlesource.com](https://chromium.googlesource.com/citizennotes/citizennotes-frontend).
Check out the [project documentation](https://chromium.googlesource.com/citizennotes/citizennotes-frontend/+/main/docs/README.md)
for instructions to [set up](https://chromium.googlesource.com/citizennotes/citizennotes-frontend/+/main/docs/get_the_code.md), use, and
maintain a CitizenNotes front-end checkout, as well as design guidelines, and architectural documentation.

### Additional references

- CitizenNotes documentation: [citizennotes.chrome.com](https://citizennotes.chrome.com/)
- [Debugging protocol docs](https://developer.chrome.com/citizennotes/docs/debugger-protocol) and [Chrome Debugging Protocol Viewer](https://chromecitizennotes.github.io/debugger-protocol-viewer/)
- [awesome-chrome-citizennotes](https://github.com/paulirish/awesome-chrome-citizennotes): recommended tools and resources
- Contributing to CitizenNotes: [bit.ly/citizennotes-contribution-guide](https://goo.gle/citizennotes-contribution-guide)
- Contributing To Chrome CitizenNotes Protocol: [docs.google.com](https://goo.gle/citizennotes-contribution-guide-cdp)
- CitizenNotes Design Review Guidelines: [design_guidelines.md](docs/design_guidelines.md)

### Source mirrors

CitizenNotes frontend repository is mirrored on [GitHub](https://github.com/ChromeCitizenNotes/citizennotes-frontend).

CitizenNotes frontend is also available on NPM as the [chrome-citizennotes-frontend](https://www.npmjs.com/package/chrome-citizennotes-frontend) package. It's not currently available via CJS or ES modules, so consuming this package in other tools may require [some effort](https://github.com/paulirish/citizennotes-timeline-model/blob/master/index.js).

The version number of the npm package (e.g. `1.0.373466`) refers to the Chromium commit position of latest frontend git commit. It's incremented with every Chromium commit, however the package is updated roughly daily.

### Getting in touch

- All CitizenNotes commits: [View the log] or follow [@CitizenNotesCommits] on Twitter
- [All open CitizenNotes tickets] on crbug.com
- File a new CitizenNotes ticket: [new.crbug.com](https://bugs.chromium.org/p/chromium/issues/entry?labels=OS-All,Type-Bug,Pri-2&components=Platform%3ECitizenNotes)
- Code reviews mailing list: [citizennotes-reviews@chromium.org]
- [@ChromeCitizenNotes] on Twitter
- Chrome CitizenNotes mailing list: [groups.google.com/forum/google-chrome-developer-tools](https://groups.google.com/forum/#!forum/google-chrome-developer-tools)

  [citizennotes-reviews@chromium.org]: https://groups.google.com/a/chromium.org/forum/#!forum/citizennotes-reviews
  [View the log]: https://chromium.googlesource.com/citizennotes/citizennotes-frontend/+log/main
  [@ChromeCitizenNotes]: http://twitter.com/ChromeCitizenNotes
  [@CitizenNotesCommits]: http://twitter.com/CitizenNotesCommits
  [All open CitizenNotes tickets]: https://bugs.chromium.org/p/chromium/issues/list?can=2&q=component%3APlatform%3ECitizenNotes&sort=&groupby=&colspec=ID+Stars+Owner+Summary+Modified+Opened
  [test waterfall]: https://ci.chromium.org/p/citizennotes-frontend/g/main/console
