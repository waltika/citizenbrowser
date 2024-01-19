# Testing Chromium CitizenNotes

Follow the steps outlined in [Get the Code](get_the_code.md) to checkout the CitizenNotes front-end code.

[TOC]

## CitizenNotes frontend

Test are available by running scripts in `scripts/test/`. Please refer to the [overview document](https://docs.google.com/document/d/1c2KLKoFMqLB2A9sNAHIhYb70XFyfBUBs5BZSYfQAT-Y/edit). The current test status can be seen at the [test waterfall](https://ci.chromium.org/p/citizennotes-frontend/g/main/console).

*   [E2E test guide](../test/e2e/README.md)
*   [Unit test guide](../test/unittests/README.md)


## Layout tests

After building content shell as part of Chromium, we can also run layout tests that are relevant for CitizenNotes frontend:

```bash
autoninja -C out/Default content_shell
third_party/blink/tools/run_web_tests.py -t Default http/tests/citizennotes
```

To debug a failing layout test we can run
```bash
npm run debug-test -- http/tests/citizennotes/<path>/<to>/<test>.js
```

The script supports either default CitizenNotes checkout inside the chromium tree or side-by-side checkouts of chromium and CitizenNotes. Passing `--custom-citizennotes-frontend` is not supported currently, meaning in the side-by-side scenario the CitizenNotes checkout inside the chromium tree will be used (if not symlinked).
