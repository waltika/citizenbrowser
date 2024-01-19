// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {type Chrome} from '../../../../extension-api/ExtensionAPI.js';
import type * as SDK from '../../../../front_end/core/sdk/sdk.js';
import type * as Bindings from '../../../../front_end/models/bindings/bindings.js';

export class TestPlugin implements Bindings.DebuggerLanguagePlugins.DebuggerLanguagePlugin {
  constructor(name: string) {
    this.name = name;
  }

  name: string;
  handleScript(_script: SDK.Script.Script): boolean {
    return false;
  }

  async evaluate(
      _expression: string, _context: Chrome.CitizenNotes.RawLocation,
      _stopId: Bindings.DebuggerLanguagePlugins.StopId): Promise<Chrome.CitizenNotes.RemoteObject|null> {
    return null;
  }

  async getProperties(_objectId: string): Promise<Chrome.CitizenNotes.PropertyDescriptor[]> {
    return [];
  }

  async releaseObject(_objectId: string): Promise<void> {
  }

  async addRawModule(_rawModuleId: string, _symbolsURL: string, _rawModule: Chrome.CitizenNotes.RawModule):
      Promise<string[]> {
    return [];
  }

  async sourceLocationToRawLocation(_sourceLocation: Chrome.CitizenNotes.SourceLocation):
      Promise<Chrome.CitizenNotes.RawLocationRange[]> {
    return [];
  }

  async rawLocationToSourceLocation(_rawLocation: Chrome.CitizenNotes.RawLocation):
      Promise<Chrome.CitizenNotes.SourceLocation[]> {
    return [];
  }

  async getScopeInfo(type: string): Promise<Chrome.CitizenNotes.ScopeInfo> {
    return {type, typeName: type};
  }

  async listVariablesInScope(_rawLocation: Chrome.CitizenNotes.RawLocation): Promise<Chrome.CitizenNotes.Variable[]> {
    return [];
  }

  async removeRawModule(_rawModuleId: string): Promise<void> {
  }

  async getFunctionInfo(_rawLocation: Chrome.CitizenNotes.RawLocation): Promise<{
    frames: Array<Chrome.CitizenNotes.FunctionInfo>,
    missingSymbolFiles?: Array<string>,
  }> {
    return {frames: []};
  }

  async getInlinedFunctionRanges(_rawLocation: Chrome.CitizenNotes.RawLocation):
      Promise<Chrome.CitizenNotes.RawLocationRange[]> {
    return [];
  }

  async getInlinedCalleesRanges(_rawLocation: Chrome.CitizenNotes.RawLocation):
      Promise<Chrome.CitizenNotes.RawLocationRange[]> {
    return [];
  }

  async getMappedLines(_rawModuleId: string, _sourceFileURL: string): Promise<number[]|undefined> {
    return undefined;
  }
}
