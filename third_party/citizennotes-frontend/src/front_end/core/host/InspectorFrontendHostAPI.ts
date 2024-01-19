// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type * as Platform from '../../core/platform/platform.js';

// TODO(crbug.com/1167717): Make this a const enum again
// eslint-disable-next-line rulesdir/const_enum
export enum Events {
  AppendedToURL = 'appendedToURL',
  CanceledSaveURL = 'canceledSaveURL',
  ContextMenuCleared = 'contextMenuCleared',
  ContextMenuItemSelected = 'contextMenuItemSelected',
  DeviceCountUpdated = 'deviceCountUpdated',
  DevicesDiscoveryConfigChanged = 'devicesDiscoveryConfigChanged',
  DevicesPortForwardingStatusChanged = 'devicesPortForwardingStatusChanged',
  DevicesUpdated = 'devicesUpdated',
  DispatchMessage = 'dispatchMessage',
  DispatchMessageChunk = 'dispatchMessageChunk',
  EnterInspectElementMode = 'enterInspectElementMode',
  EyeDropperPickedColor = 'eyeDropperPickedColor',
  FileSystemsLoaded = 'fileSystemsLoaded',
  FileSystemRemoved = 'fileSystemRemoved',
  FileSystemAdded = 'fileSystemAdded',
  FileSystemFilesChangedAddedRemoved = 'FileSystemFilesChangedAddedRemoved',
  IndexingTotalWorkCalculated = 'indexingTotalWorkCalculated',
  IndexingWorked = 'indexingWorked',
  IndexingDone = 'indexingDone',
  KeyEventUnhandled = 'keyEventUnhandled',
  ReattachRootTarget = 'reattachMainTarget',
  ReloadInspectedPage = 'reloadInspectedPage',
  RevealSourceLine = 'revealSourceLine',
  SavedURL = 'savedURL',
  SearchCompleted = 'searchCompleted',
  SetInspectedTabId = 'setInspectedTabId',
  SetUseSoftMenu = 'setUseSoftMenu',
  ShowPanel = 'showPanel',
}

// TODO(crbug.com/1167717): Make this a const enum again
// eslint-disable-next-line rulesdir/const_enum
export const EventDescriptors = [
  [Events.AppendedToURL, 'appendedToURL', ['url']],
  [Events.CanceledSaveURL, 'canceledSaveURL', ['url']],
  [Events.ContextMenuCleared, 'contextMenuCleared', []],
  [Events.ContextMenuItemSelected, 'contextMenuItemSelected', ['id']],
  [Events.DeviceCountUpdated, 'deviceCountUpdated', ['count']],
  [Events.DevicesDiscoveryConfigChanged, 'devicesDiscoveryConfigChanged', ['config']],
  [Events.DevicesPortForwardingStatusChanged, 'devicesPortForwardingStatusChanged', ['status']],
  [Events.DevicesUpdated, 'devicesUpdated', ['devices']],
  [Events.DispatchMessage, 'dispatchMessage', ['messageObject']],
  [Events.DispatchMessageChunk, 'dispatchMessageChunk', ['messageChunk', 'messageSize']],
  [Events.EnterInspectElementMode, 'enterInspectElementMode', []],
  [Events.EyeDropperPickedColor, 'eyeDropperPickedColor', ['color']],
  [Events.FileSystemsLoaded, 'fileSystemsLoaded', ['fileSystems']],
  [Events.FileSystemRemoved, 'fileSystemRemoved', ['fileSystemPath']],
  [Events.FileSystemAdded, 'fileSystemAdded', ['errorMessage', 'fileSystem']],
  [Events.FileSystemFilesChangedAddedRemoved, 'fileSystemFilesChangedAddedRemoved', ['changed', 'added', 'removed']],
  [Events.IndexingTotalWorkCalculated, 'indexingTotalWorkCalculated', ['requestId', 'fileSystemPath', 'totalWork']],
  [Events.IndexingWorked, 'indexingWorked', ['requestId', 'fileSystemPath', 'worked']],
  [Events.IndexingDone, 'indexingDone', ['requestId', 'fileSystemPath']],
  [Events.KeyEventUnhandled, 'keyEventUnhandled', ['event']],
  [Events.ReattachRootTarget, 'reattachMainTarget', []],
  [Events.ReloadInspectedPage, 'reloadInspectedPage', ['hard']],
  [Events.RevealSourceLine, 'revealSourceLine', ['url', 'lineNumber', 'columnNumber']],
  [Events.SavedURL, 'savedURL', ['url', 'fileSystemPath']],
  [Events.SearchCompleted, 'searchCompleted', ['requestId', 'fileSystemPath', 'files']],
  [Events.SetInspectedTabId, 'setInspectedTabId', ['tabId']],
  [Events.SetUseSoftMenu, 'setUseSoftMenu', ['useSoftMenu']],
  [Events.ShowPanel, 'showPanel', ['panelName']],
];

export interface DispatchMessageChunkEvent {
  messageChunk: string;
  messageSize: number;
}

export interface EyeDropperPickedColorEvent {
  r: number;
  g: number;
  b: number;
  a: number;
}

export interface CitizenNotesFileSystem {
  type: string;
  fileSystemName: string;
  rootURL: string;
  fileSystemPath: Platform.CitizenNotesPath.RawPathString;
}

export interface FileSystemAddedEvent {
  errorMessage?: string;
  fileSystem: CitizenNotesFileSystem|null;
}

export interface FilesChangedEvent {
  changed: Platform.CitizenNotesPath.RawPathString[];
  added: Platform.CitizenNotesPath.RawPathString[];
  removed: Platform.CitizenNotesPath.RawPathString[];
}

export interface IndexingEvent {
  requestId: number;
  fileSystemPath: string;
}

export interface IndexingTotalWorkCalculatedEvent extends IndexingEvent {
  totalWork: number;
}

export interface IndexingWorkedEvent extends IndexingEvent {
  worked: number;
}

export interface KeyEventUnhandledEvent {
  type: string;
  key: string;
  keyCode: number;
  modifiers: number;
}

export interface RevealSourceLineEvent {
  url: Platform.CitizenNotesPath.UrlString;
  lineNumber: number;
  columnNumber: number;
}

export interface SavedURLEvent {
  url: Platform.CitizenNotesPath.RawPathString|Platform.CitizenNotesPath.UrlString;
  fileSystemPath: Platform.CitizenNotesPath.RawPathString|Platform.CitizenNotesPath.UrlString;
}

export interface SearchCompletedEvent {
  requestId: number;
  files: Platform.CitizenNotesPath.RawPathString[];
}

export interface DoAidaConversationResult {
  response: string;
}

export interface VisualElementImpression {
  id: number;
  type: number;
  parent?: number;
  context?: number;
}

export interface ImpressionEvent {
  impressions: VisualElementImpression[];
}

export interface ClickEvent {
  veid: number;
  mouseButton: number;
  context?: number;
  doubleClick: boolean;
}

export interface HoverEvent {
  veid: number;
  time?: number;
  context?: number;
}

export interface DragEvent {
  veid: number;
  context?: number;
}

export interface ChangeEvent {
  veid: number;
  context?: number;
}

export interface KeyDownEvent {
  veid: number;
  context?: number;
}

// While `EventDescriptors` are used to dynamically dispatch host binding events,
// the `EventTypes` "type map" is used for type-checking said events by TypeScript.
// `EventTypes` is not used at runtime.
// Please note that the "dispatch" side can't be type-checked as the dispatch is
// done dynamically.
export type EventTypes = {
  [Events.AppendedToURL]: Platform.CitizenNotesPath.RawPathString|Platform.CitizenNotesPath.UrlString,
  [Events.CanceledSaveURL]: Platform.CitizenNotesPath.UrlString,
  [Events.ContextMenuCleared]: void,
  [Events.ContextMenuItemSelected]: number,
  [Events.DeviceCountUpdated]: number,
  [Events.DevicesDiscoveryConfigChanged]: Adb.Config,
  [Events.DevicesPortForwardingStatusChanged]: void,
  [Events.DevicesUpdated]: void,
  [Events.DispatchMessage]: string,
  [Events.DispatchMessageChunk]: DispatchMessageChunkEvent,
  [Events.EnterInspectElementMode]: void,
  [Events.EyeDropperPickedColor]: EyeDropperPickedColorEvent,
  [Events.FileSystemsLoaded]: CitizenNotesFileSystem[],
  [Events.FileSystemRemoved]: Platform.CitizenNotesPath.RawPathString,
  [Events.FileSystemAdded]: FileSystemAddedEvent,
  [Events.FileSystemFilesChangedAddedRemoved]: FilesChangedEvent,
  [Events.IndexingTotalWorkCalculated]: IndexingTotalWorkCalculatedEvent,
  [Events.IndexingWorked]: IndexingWorkedEvent,
  [Events.IndexingDone]: IndexingEvent,
  [Events.KeyEventUnhandled]: KeyEventUnhandledEvent,
  [Events.ReattachRootTarget]: void,
  [Events.ReloadInspectedPage]: boolean,
  [Events.RevealSourceLine]: RevealSourceLineEvent,
  [Events.SavedURL]: SavedURLEvent,
  [Events.SearchCompleted]: SearchCompletedEvent,
  [Events.SetInspectedTabId]: string,
  [Events.SetUseSoftMenu]: boolean,
  [Events.ShowPanel]: string,
};

export interface InspectorFrontendHostAPI {
  addFileSystem(type?: string): void;

  loadCompleted(): void;

  indexPath(requestId: number, fileSystemPath: Platform.CitizenNotesPath.RawPathString, excludedFolders: string): void;

  /**
   * Requests inspected page to be placed atop of the inspector frontend with specified bounds.
   */
  setInspectedPageBounds(bounds: {
    x: number,
    y: number,
    width: number,
    height: number,
  }): void;

  showCertificateViewer(certChain: string[]): void;

  setWhitelistedShortcuts(shortcuts: string): void;

  setEyeDropperActive(active: boolean): void;

  inspectElementCompleted(): void;

  openInNewTab(url: Platform.CitizenNotesPath.UrlString): void;

  showItemInFolder(fileSystemPath: Platform.CitizenNotesPath.RawPathString): void;

  removeFileSystem(fileSystemPath: Platform.CitizenNotesPath.RawPathString): void;

  requestFileSystems(): void;

  save(url: Platform.CitizenNotesPath.UrlString, content: string, forceSaveAs: boolean): void;

  append(url: Platform.CitizenNotesPath.UrlString, content: string): void;

  close(url: Platform.CitizenNotesPath.UrlString): void;

  searchInPath(requestId: number, fileSystemPath: Platform.CitizenNotesPath.RawPathString, query: string): void;

  stopIndexing(requestId: number): void;

  bringToFront(): void;

  closeWindow(): void;

  copyText(text: string|null|undefined): void;

  inspectedURLChanged(url: Platform.CitizenNotesPath.UrlString): void;

  isolatedFileSystem(fileSystemId: string, registeredName: string): FileSystem|null;

  loadNetworkResource(
      url: string, headers: string, streamId: number, callback: (arg0: LoadNetworkResourceResult) => void): void;

  registerPreference(name: string, options: {synced?: boolean}): void;

  getPreferences(callback: (arg0: {
                   [x: string]: string,
                 }) => void): void;

  getPreference(name: string, callback: (arg0: string) => void): void;

  setPreference(name: string, value: string): void;

  removePreference(name: string): void;

  clearPreferences(): void;

  getSyncInformation(callback: (arg0: SyncInformation) => void): void;

  upgradeDraggedFileSystemPermissions(fileSystem: FileSystem): void;

  platform(): string;

  recordCountHistogram(histogramName: string, sample: number, min: number, exclusiveMax: number, bucketSize: number):
      void;

  recordEnumeratedHistogram(actionName: EnumeratedHistogram, actionCode: number, bucketSize: number): void;

  recordPerformanceHistogram(histogramName: string, duration: number): void;

  recordUserMetricsAction(umaName: string): void;

  sendMessageToBackend(message: string): void;

  setDevicesDiscoveryConfig(config: Adb.Config): void;

  setDevicesUpdatesEnabled(enabled: boolean): void;

  performActionOnRemotePage(pageId: string, action: string): void;

  openRemotePage(browserId: string, url: string): void;

  openNodeFrontend(): void;

  setInjectedScriptForOrigin(origin: string, script: string): void;

  setIsDocked(isDocked: boolean, callback: () => void): void;

  showSurvey(trigger: string, callback: (arg0: ShowSurveyResult) => void): void;

  canShowSurvey(trigger: string, callback: (arg0: CanShowSurveyResult) => void): void;

  zoomFactor(): number;

  zoomIn(): void;

  zoomOut(): void;

  resetZoom(): void;

  showContextMenuAtPoint(x: number, y: number, items: ContextMenuDescriptor[], document: Document): void;

  reattach(callback: () => void): void;

  readyForTest(): void;

  connectionReady(): void;

  setOpenNewWindowForPopups(value: boolean): void;

  isHostedMode(): boolean;

  setAddExtensionCallback(callback: (arg0: ExtensionDescriptor) => void): void;

  initialTargetId(): Promise<string|null>;

  doAidaConversation: (request: string, cb: (result: DoAidaConversationResult) => void) => void;

  recordImpression(event: ImpressionEvent): void;
  recordClick(event: ClickEvent): void;
  recordHover(event: HoverEvent): void;
  recordDrag(event: DragEvent): void;
  recordChange(event: ChangeEvent): void;
  recordKeyDown(event: KeyDownEvent): void;
}

export interface ContextMenuDescriptor {
  type: 'checkbox'|'item'|'separator'|'subMenu';
  id?: number;
  label?: string;
  enabled?: boolean;
  checked?: boolean;
  subItems?: ContextMenuDescriptor[];
  jslogContext?: string;
}
export interface LoadNetworkResourceResult {
  statusCode: number;
  headers?: {
    [x: string]: string,
  };
  netError?: number;
  netErrorName?: string;
  urlValid?: boolean;
  messageOverride?: string;
}
export interface ExtensionDescriptor {
  startPage: string;
  name: string;
  exposeExperimentalAPIs: boolean;
  hostsPolicy?: ExtensionHostsPolicy;
  allowFileAccess?: boolean;
}
export interface ExtensionHostsPolicy {
  runtimeAllowedHosts: string[];
  runtimeBlockedHosts: string[];
}
export interface ShowSurveyResult {
  surveyShown: boolean;
}
export interface CanShowSurveyResult {
  canShowSurvey: boolean;
}
export interface SyncInformation {
  /** Whether Chrome Sync is enabled and active */
  isSyncActive: boolean;
  /** Whether syncing of Chrome Settings is enabled via Chrome Sync is enabled */
  arePreferencesSynced?: boolean;
  /** The email of the account used for syncing */
  accountEmail?: string;
  /** The image of the account used for syncing. Its a base64 encoded PNG */
  accountImage?: string;
}

/**
 * Enum for recordPerformanceHistogram
 * Warning: There is another definition of this enum in the CitizenNotes code
 * base, keep them in sync:
 * front_end/citizennotes_compatibility.js
 * @readonly
 */
// TODO(crbug.com/1167717): Make this a const enum again
// eslint-disable-next-line rulesdir/const_enum
export enum EnumeratedHistogram {
  ActionTaken = 'CitizenNotes.ActionTaken',
  BreakpointWithConditionAdded = 'CitizenNotes.BreakpointWithConditionAdded',
  BreakpointEditDialogRevealedFrom = 'CitizenNotes.BreakpointEditDialogRevealedFrom',
  PanelClosed = 'CitizenNotes.PanelClosed',
  PanelShown = 'CitizenNotes.PanelShown',
  SidebarPaneShown = 'CitizenNotes.SidebarPaneShown',
  KeyboardShortcutFired = 'CitizenNotes.KeyboardShortcutFired',
  IssueCreated = 'CitizenNotes.IssueCreated',
  IssuesPanelIssueExpanded = 'CitizenNotes.IssuesPanelIssueExpanded',
  IssuesPanelOpenedFrom = 'CitizenNotes.IssuesPanelOpenedFrom',
  IssuesPanelResourceOpened = 'CitizenNotes.IssuesPanelResourceOpened',
  KeybindSetSettingChanged = 'CitizenNotes.KeybindSetSettingChanged',
  ElementsSidebarTabShown = 'CitizenNotes.Elements.SidebarTabShown',
  ExperimentEnabledAtLaunch = 'CitizenNotes.ExperimentEnabledAtLaunch',
  ExperimentDisabledAtLaunch = 'CitizenNotes.ExperimentDisabledAtLaunch',
  ExperimentEnabled = 'CitizenNotes.ExperimentEnabled',
  ExperimentDisabled = 'CitizenNotes.ExperimentDisabled',
  DeveloperResourceLoaded = 'CitizenNotes.DeveloperResourceLoaded',
  DeveloperResourceScheme = 'CitizenNotes.DeveloperResourceScheme',
  LinearMemoryInspectorRevealedFrom = 'CitizenNotes.LinearMemoryInspector.RevealedFrom',
  LinearMemoryInspectorTarget = 'CitizenNotes.LinearMemoryInspector.Target',
  Language = 'CitizenNotes.Language',
  SyncSetting = 'CitizenNotes.SyncSetting',
  RecordingAssertion = 'CitizenNotes.RecordingAssertion',
  RecordingCodeToggled = 'CitizenNotes.RecordingCodeToggled',
  RecordingCopiedToClipboard = 'CitizenNotes.RecordingCopiedToClipboard',
  RecordingEdited = 'CitizenNotes.RecordingEdited',
  RecordingExported = 'CitizenNotes.RecordingExported',
  RecordingReplayFinished = 'CitizenNotes.RecordingReplayFinished',
  RecordingReplaySpeed = 'CitizenNotes.RecordingReplaySpeed',
  RecordingReplayStarted = 'CitizenNotes.RecordingReplayStarted',
  RecordingToggled = 'CitizenNotes.RecordingToggled',
  SourcesSidebarTabShown = 'CitizenNotes.Sources.SidebarTabShown',
  SourcesPanelFileDebugged = 'CitizenNotes.SourcesPanelFileDebugged',
  SourcesPanelFileOpened = 'CitizenNotes.SourcesPanelFileOpened',
  NetworkPanelResponsePreviewOpened = 'CitizenNotes.NetworkPanelResponsePreviewOpened',
  StyleTextCopied = 'CitizenNotes.StyleTextCopied',
  ManifestSectionSelected = 'CitizenNotes.ManifestSectionSelected',
  CSSHintShown = 'CitizenNotes.CSSHintShown',
  LighthouseModeRun = 'CitizenNotes.LighthouseModeRun',
  LighthouseCategoryUsed = 'CitizenNotes.LighthouseCategoryUsed',
  ColorConvertedFrom = 'CitizenNotes.ColorConvertedFrom',
  ColorPickerOpenedFrom = 'CitizenNotes.ColorPickerOpenedFrom',
  CSSPropertyDocumentation = 'CitizenNotes.CSSPropertyDocumentation',
  InlineScriptParsed = 'CitizenNotes.InlineScriptParsed',
  VMInlineScriptTypeShown = 'CitizenNotes.VMInlineScriptShown',
  BreakpointsRestoredFromStorageCount = 'CitizenNotes.BreakpointsRestoredFromStorageCount',
  SwatchActivated = 'CitizenNotes.SwatchActivated',
  BadgeActivated = 'CitizenNotes.BadgeActivated',
  AnimationPlaybackRateChanged = 'CitizenNotes.AnimationPlaybackRateChanged',
  AnimationPointDragged = 'CitizenNotes.AnimationPointDragged',
  LegacyResourceTypeFilterNumberOfSelectedChanged = 'CitizenNotes.LegacyResourceTypeFilterNumberOfSelectedChanged',
  LegacyResourceTypeFilterItemSelected = 'CitizenNotes.LegacyResourceTypeFilterItemSelected',
  ResourceTypeFilterNumberOfSelectedChanged = 'CitizenNotes.ResourceTypeFilterNumberOfSelectedChanged',
  ResourceTypeFilterItemSelected = 'CitizenNotes.ResourceTypeFilterItemSelected',
  NetworkPanelMoreFiltersNumberOfSelectedChanged = 'CitizenNotes.NetworkPanelMoreFiltersNumberOfSelectedChanged',
  NetworkPanelMoreFiltersItemSelected = 'CitizenNotes.NetworkPanelMoreFiltersItemSelected',
}
