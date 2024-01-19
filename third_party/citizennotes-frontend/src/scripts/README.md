# CitizenNotes Scripts

## Development workflow scripts

These are scripts that can be useful to run independently as you're working on Chrome CitizenNotes front-end.

The newer scripts such as for testing and hosted mode are written in Node.js, which has become the standard toolchain for web apps. The older scripts such as building (e.g. bundling and minifying) are written in Python, which has first-class support in Chromium's infrastructure.

## Overview

### Folders

- build - Python package for generating CitizenNotes debug and release mode
- chrome_debug_launcher - automagically finds Chrome Canary and launches it with debugging flags (e.g. remote debugging port)
- closure - see section on Closure Compiler below
- gulp - experimental build process written in node.js & gulp to remove the dependency on Chromium-specific build tools (i.e. gn and ninja)
- hosted_mode - run CitizenNotes on a localhost development server

### Python Scripts
- compile_frontend.py - runs closure compiler to do static type analysis
    - Note: the compiled outputs are not actually used to run CitizenNotes
- lint_javascript.py - run eslint

### Node.js scripts

The easiest way to run the node.js scripts is to use `npm run` which displays all the commands. For more information on the specific `npm run` commands, take a look at the primary citizennotes front-end readme (`../readme.md`).
