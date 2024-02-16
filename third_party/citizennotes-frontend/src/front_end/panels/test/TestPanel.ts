// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2009 Joseph Pecoraro
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import type * as Common from '../../core/common/common.js';
import * as UI from '../../ui/legacy/legacy.js';

import {TestView} from '../test/TestView.js';

let testPanelInstance: TestPanel;

export class TestPanel extends UI.Panel.Panel {
  private readonly view: TestView;
  constructor() {
    super('test');
    this.view = TestView.instance();
  }

  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): TestPanel {
    const {forceNew} = opts;
    if (!testPanelInstance || forceNew) {
      testPanelInstance = new TestPanel();
    }

    return testPanelInstance;
  }

  static updateContextFlavor(): void {
    const testView = TestPanel.instance().view;
    UI.Context.Context.instance().setFlavor(TestView, testView.isShowing() ? testView : null);
  }

  override wasShown(): void {
    super.wasShown();
    const wrapper = wrapperViewInstance;
    if (wrapper && wrapper.isShowing()) {
      UI.InspectorView.InspectorView.instance().setDrawerMinimized(true);
    }
    this.view.show(this.element);
    TestPanel.updateContextFlavor();
  }

  override willHide(): void {
    super.willHide();
    // The minimized drawer has 0 height, and showing Test inside may set
    // Test's scrollTop to 0. Unminimize before calling show to avoid this.
    UI.InspectorView.InspectorView.instance().setDrawerMinimized(false);
    if (wrapperViewInstance) {
      wrapperViewInstance.showViewInWrapper();
    }
    TestPanel.updateContextFlavor();
  }

  override searchableView(): UI.SearchableView.SearchableView|null {
    return TestView.instance().searchableView();
  }
}

let wrapperViewInstance: WrapperView|null = null;

export class WrapperView extends UI.Widget.VBox {
  private readonly view: TestView;

  private constructor() {
    super();
    this.view = TestView.instance();
  }

  static instance(): WrapperView {
    if (!wrapperViewInstance) {
      wrapperViewInstance = new WrapperView();
    }
    return wrapperViewInstance;
  }

  override wasShown(): void {
    if (!TestPanel.instance().isShowing()) {
      this.showViewInWrapper();
    } else {
      UI.InspectorView.InspectorView.instance().setDrawerMinimized(true);
    }
    TestPanel.updateContextFlavor();
  }

  override willHide(): void {
    UI.InspectorView.InspectorView.instance().setDrawerMinimized(false);
    TestPanel.updateContextFlavor();
  }

  showViewInWrapper(): void {
    this.view.show(this.element);
  }
}

export class TestRevealer implements Common.Revealer.Revealer<Common.Test.Test> {
  async reveal(_object: Common.Test.Test): Promise<void> {
    const testView = TestView.instance();
    if (testView.isShowing()) {
      testView.focus();
      return;
    }
    await UI.ViewManager.ViewManager.instance().showView('test-view');
  }
}
