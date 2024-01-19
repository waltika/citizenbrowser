// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const rule = require('../lib/check_component_naming.js');
const ruleTester = new (require('eslint').RuleTester)({
  parserOptions: {ecmaVersion: 9, sourceType: 'module'},
  parser: require.resolve('@typescript-eslint/parser'),
});

ruleTester.run('check_component_naming', rule, {
  valid: [
    {
      code: `export class Foo extends HTMLElement {
        static readonly litTagName = LitHtml.literal\`citizennotes-foo\`
      }

      ComponentHelpers.CustomElements.defineComponent('citizennotes-foo', Foo);

      declare global {
        interface HTMLElementTagNameMap {
          'citizennotes-foo': Foo
        }
      }`,
      filename: 'front_end/ui/components/Foo.ts'
    },
    {
      // Multiple components in one file is valid.
      code: `export class Foo extends HTMLElement {
        static readonly litTagName = LitHtml.literal\`citizennotes-foo\`
      }

      export class Bar extends HTMLElement {
        static readonly litTagName = LitHtml.literal\`citizennotes-bar\`
      }

      ComponentHelpers.CustomElements.defineComponent('citizennotes-bar', Bar);
      ComponentHelpers.CustomElements.defineComponent('citizennotes-foo', Foo);

      declare global {
        interface HTMLElementTagNameMap {
          'citizennotes-foo': Foo
          'citizennotes-bar': Bar
        }
      }`,
      filename: 'front_end/ui/components/Foo.ts'
    },
    {
      code: `export class Foo extends HTMLElement {
        static readonly litTagName = LitHtml.literal\`citizennotes-foo\`
      }

      defineComponent('citizennotes-foo', Foo);

      declare global {
        interface HTMLElementTagNameMap {
          'citizennotes-foo': Foo
        }
      }`,
      filename: 'front_end/ui/components/Foo.ts'
    }
  ],
  invalid: [
    // Missing static litTagName
    {
      code: `export class Foo extends HTMLElement {
      }

      ComponentHelpers.CustomElements.defineComponent('citizennotes-not-foo', Foo);

      declare global {
        interface HTMLElementTagNameMap {
          'citizennotes-foo': Foo
        }
      }`,
      filename: 'front_end/ui/components/Foo.ts',
      errors: [{messageId: 'noStaticTagName'}]
    },
    // static is not a string
    {
      code: `export class Foo extends HTMLElement {
        static readonly litTagName = LitHtml.literal\`\${someVar}\`
      }

      ComponentHelpers.CustomElements.defineComponent('citizennotes-foo', Foo);

      declare global {
        interface HTMLElementTagNameMap {
          'citizennotes-foo': Foo
        }
      }`,
      filename: 'front_end/ui/components/Foo.ts',
      errors: [{
        messageId: 'staticLiteralInvalid'

      }]
    },
    // Static is not readonly
    {
      code: `export class Foo extends HTMLElement {
        static litTagName = LitHtml.literal\`citizennotes-foo\`;
      }

      ComponentHelpers.CustomElements.defineComponent('citizennotes-foo', Foo);

      declare global {
        interface HTMLElementTagNameMap {
          'citizennotes-foo': Foo
        }
      }`,
      filename: 'front_end/ui/components/Foo.ts',
      errors: [{
        messageId: 'staticLiteralNotReadonly'

      }]
    },
    // defineComponent call uses wrong name
    {
      code: `export class Foo extends HTMLElement {
        static readonly litTagName = LitHtml.literal\`citizennotes-foo\`
      }

      ComponentHelpers.CustomElements.defineComponent('citizennotes-not-foo', Foo);

      declare global {
        interface HTMLElementTagNameMap {
          'citizennotes-foo': Foo
        }
      }`,
      filename: 'front_end/ui/components/Foo.ts',
      errors: [{messageId: 'noDefineCall'}]
    },
    // defineComponent call uses non literal
    {
      code: `export class Foo extends HTMLElement {
        static readonly litTagName = LitHtml.literal\`citizennotes-foo\`
      }

      const name = 'citizennotes-foo';
      ComponentHelpers.CustomElements.defineComponent(name, Foo);

      declare global {
        interface HTMLElementTagNameMap {
          'citizennotes-foo': Foo
        }
      }`,
      filename: 'front_end/ui/components/Foo.ts',
      errors: [{
        messageId: 'defineCallNonLiteral'

      }]
    },
    // defineComponent call missing
    {
      code: `export class Foo extends HTMLElement {
        static readonly litTagName = LitHtml.literal\`citizennotes-foo\`
      }

      declare global {
        interface HTMLElementTagNameMap {
          'citizennotes-foo': Foo
        }
      }`,
      filename: 'front_end/ui/components/Foo.ts',
      errors: [{
        messageId: 'noDefineCall'

      }]
    },
    // TS interface is incorrect
    {
      code: `export class Foo extends HTMLElement {
        static readonly litTagName = LitHtml.literal\`citizennotes-foo\`
      }

      ComponentHelpers.CustomElements.defineComponent('citizennotes-foo', Foo);

      declare global {
        interface HTMLElementTagNameMap {
          'citizennotes-not-foo': Foo
        }
      }`,
      filename: 'front_end/ui/components/Foo.ts',
      errors: [{messageId: 'noTSInterface'}]
    },
    // TS interface is missing
    {
      code: `export class Foo extends HTMLElement {
        static readonly litTagName = LitHtml.literal\`citizennotes-foo\`
      }

      ComponentHelpers.CustomElements.defineComponent('citizennotes-foo', Foo);`,
      filename: 'front_end/ui/components/Foo.ts',
      errors: [{messageId: 'noTSInterface', data: {tagName: 'citizennotes-foo'}}]
    },
    {
      // Not using LitHtml.literal
      code: `export class Foo extends HTMLElement {
        static readonly litTagName = 'citizennotes-foo';
      }

      ComponentHelpers.CustomElements.defineComponent('citizennotes-foo', Foo);

      declare global {
        interface HTMLElementTagNameMap {
          'citizennotes-foo': Foo
        }
      }`,
      filename: 'front_end/ui/components/Foo.ts',
      errors: [{messageId: 'litTagNameNotLiteral'}]
    },
    {
      // Multiple components in one file is valid.
      // But here citizennotes-foo is fine, but citizennotes-bar is missing a define call
      code: `export class Foo extends HTMLElement {
        static readonly litTagName = LitHtml.literal\`citizennotes-foo\`
      }

      export class Bar extends HTMLElement {
        static readonly litTagName = LitHtml.literal\`citizennotes-bar\`
      }

      ComponentHelpers.CustomElements.defineComponent('citizennotes-foo', Foo);

      declare global {
        interface HTMLElementTagNameMap {
          'citizennotes-foo': Foo
          'citizennotes-bar': Bar
        }
      }`,
      filename: 'front_end/ui/components/Foo.ts',
      errors: [{messageId: 'noDefineCall', data: {tagName: 'citizennotes-bar'}}]
    },
    {
      // Multiple components in one file is valid.
      // But here citizennotes-foo is fine, but citizennotes-bar has the wrong static tag name
      code: `export class Foo extends HTMLElement {
        static readonly litTagName = LitHtml.literal\`citizennotes-foo\`
      }

      export class Bar extends HTMLElement {
        static readonly litTagName = LitHtml.literal\`citizennotes-foo\`
      }

      ComponentHelpers.CustomElements.defineComponent('citizennotes-foo', Foo);
      ComponentHelpers.CustomElements.defineComponent('citizennotes-bar', Foo);

      declare global {
        interface HTMLElementTagNameMap {
          'citizennotes-foo': Foo
          'citizennotes-bar': Bar
        }
      }`,
      filename: 'front_end/ui/components/Foo.ts',
      errors: [{messageId: 'duplicateStaticLitTagName', data: {tagName: 'citizennotes-foo'}}]
    },
  ]
});
