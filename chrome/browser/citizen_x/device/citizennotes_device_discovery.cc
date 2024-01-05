// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/citizen_x/device/citizennotes_device_discovery.h"

#include <map>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/citizen_x/citizennotes_window.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/citizennotes_agent_host.h"
#include "content/public/browser/citizennotes_external_agent_proxy.h"
#include "content/public/browser/citizennotes_external_agent_proxy_delegate.h"

using content::BrowserThread;
using content::CitizenNotesAgentHost;
using RemoteBrowser = CitizenNotesDeviceDiscovery::RemoteBrowser;
using RemoteDevice = CitizenNotesDeviceDiscovery::RemoteDevice;
using RemotePage = CitizenNotesDeviceDiscovery::RemotePage;

namespace {

const char kPageListRequest[] = "/json/list";
const char kVersionRequest[] = "/json/version";
const char kClosePageRequest[] = "/json/close/%s";
const char kActivatePageRequest[] = "/json/activate/%s";
const char kBrowserTargetSocket[] = "/citizennotes/browser";
const int kPollingIntervalMs = 1000;

const char kPageReloadCommand[] = "{'method': 'Page.reload', id: 1}";

const char kWebViewSocketPrefix[] = "webview_citizennotes_remote";

static void ScheduleTaskDefault(base::OnceClosure task) {
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE, std::move(task), base::Milliseconds(kPollingIntervalMs));
}

// ProtocolCommand ------------------------------------------------------------

class ProtocolCommand
    : public CNAndroidDeviceManager::AndroidWebSocket::Delegate {
 public:
  ProtocolCommand(scoped_refptr<CNAndroidDeviceManager::Device> device,
                  const std::string& socket,
                  const std::string& target_path,
                  const std::string& command);

  ProtocolCommand(const ProtocolCommand&) = delete;
  ProtocolCommand& operator=(const ProtocolCommand&) = delete;

 private:
  void OnSocketOpened() override;
  void OnFrameRead(const std::string& message) override;
  void OnSocketClosed() override;
  ~ProtocolCommand() override;

  const std::string command_;
  std::unique_ptr<CNAndroidDeviceManager::AndroidWebSocket> web_socket_;
};

ProtocolCommand::ProtocolCommand(
    scoped_refptr<CNAndroidDeviceManager::Device> device,
    const std::string& socket,
    const std::string& target_path,
    const std::string& command)
    : command_(command),
      web_socket_(device->CreateWebSocket(socket, target_path, this)) {}

void ProtocolCommand::OnSocketOpened() {
  web_socket_->SendFrame(command_);
}

void ProtocolCommand::OnFrameRead(const std::string& message) {
  delete this;
}

void ProtocolCommand::OnSocketClosed() {
  delete this;
}

ProtocolCommand::~ProtocolCommand() {}

// AgentHostDelegate ----------------------------------------------------------

class WebSocketProxy : public CNAndroidDeviceManager::AndroidWebSocket::Delegate {
 public:
  explicit WebSocketProxy(content::CitizenNotesExternalAgentProxy* proxy)
      : socket_opened_(false), proxy_(proxy) {}

  WebSocketProxy(const WebSocketProxy&) = delete;
  WebSocketProxy& operator=(const WebSocketProxy&) = delete;

  void WebSocketCreated(CNAndroidDeviceManager::AndroidWebSocket* web_socket) {
    web_socket_.reset(web_socket);
  }

  void SendMessageToBackend(std::string message) {
    if (socket_opened_) {
      web_socket_->SendFrame(std::move(message));
    } else {
      pending_messages_.push_back(std::move(message));
    }
  }

  void OnSocketOpened() override {
    socket_opened_ = true;
    for (std::string& message : pending_messages_) {
      SendMessageToBackend(std::move(message));
    }
    pending_messages_.clear();
  }

  void OnFrameRead(const std::string& message) override {
    proxy_->DispatchOnClientHost(base::as_bytes(base::make_span(message)));
  }

  void OnSocketClosed() override {
    constexpr char kMsg[] =
        "{\"method\":\"Inspector.detached\",\"params\":"
        "{\"reason\":\"Connection lost.\"}}";
    proxy_->DispatchOnClientHost(
        base::as_bytes(base::make_span(kMsg, strlen(kMsg))));
    web_socket_.reset();
    socket_opened_ = false;
    proxy_->ConnectionClosed();  // Deletes |this|.
  }

 private:
  bool socket_opened_;
  std::vector<std::string> pending_messages_;
  std::unique_ptr<CNAndroidDeviceManager::AndroidWebSocket> web_socket_;
  RAW_PTR_EXCLUSION content::CitizenNotesExternalAgentProxy* proxy_;
};

class AgentHostDelegate : public content::CitizenNotesExternalAgentProxyDelegate {
 public:
  static scoped_refptr<content::CitizenNotesAgentHost> GetOrCreateAgentHost(
      scoped_refptr<CNAndroidDeviceManager::Device> device,
      const std::string& browser_id,
      const std::string& browser_version,
      const std::string& local_id,
      const std::string& target_path,
      const std::string& type,
      const base::Value::Dict* value);

  AgentHostDelegate(const AgentHostDelegate&) = delete;
  AgentHostDelegate& operator=(const AgentHostDelegate&) = delete;

  ~AgentHostDelegate() override;

 private:
  AgentHostDelegate(scoped_refptr<CNAndroidDeviceManager::Device> device,
                    const std::string& browser_id,
                    const std::string& browser_version,
                    const std::string& local_id,
                    const std::string& target_path,
                    const std::string& type,
                    const base::Value::Dict* value);
  // CitizenNotesExternalAgentProxyDelegate overrides.
  void Attach(content::CitizenNotesExternalAgentProxy* proxy) override;
  void Detach(content::CitizenNotesExternalAgentProxy* proxy) override;
  std::string GetType() override;
  std::string GetTitle() override;
  std::string GetDescription() override;
  GURL GetURL() override;
  GURL GetFaviconURL() override;
  std::string GetFrontendURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;
  base::TimeTicks GetLastActivityTime() override;
  void SendMessageToBackend(content::CitizenNotesExternalAgentProxy* proxy,
                            base::span<const uint8_t> message) override;

  scoped_refptr<CNAndroidDeviceManager::Device> device_;
  std::string browser_id_;
  std::string local_id_;
  std::string target_path_;
  std::string remote_type_;
  std::string remote_id_;
  std::string frontend_url_;
  std::string title_;
  std::string description_;
  GURL url_;
  GURL favicon_url_;
  RAW_PTR_EXCLUSION content::CitizenNotesAgentHost* agent_host_;
  std::map<content::CitizenNotesExternalAgentProxy*,
           std::unique_ptr<WebSocketProxy>>
      proxies_;
};

static std::string GetStringProperty(const base::Value::Dict& value,
                                     const std::string& name) {
  const std::string* result = value.FindString(name);
  return result ? *result : std::string();
}

static std::string BuildUniqueTargetId(const std::string& serial,
                                       const std::string& browser_id,
                                       const base::Value::Dict& value) {
  return base::StringPrintf("%s:%s:%s", serial.c_str(), browser_id.c_str(),
                            GetStringProperty(value, "id").c_str());
}

static std::string GetFrontendURLFromValue(const base::Value::Dict& value,
                                           const std::string& browser_version) {
  std::string frontend_url = GetStringProperty(value, "citizennotesFrontendUrl");
  size_t ws_param = frontend_url.find("?ws");
  if (ws_param != std::string::npos) {
    frontend_url = frontend_url.substr(0, ws_param);
  }
  if (base::StartsWith(frontend_url, "http:", base::CompareCase::SENSITIVE)) {
    frontend_url = "https:" + frontend_url.substr(5);
  }
  if (!browser_version.empty()) {
    frontend_url += "?remoteVersion=" + browser_version;
  }
  return frontend_url;
}

static std::string GetTargetPath(const base::Value::Dict& value) {
  std::string target_path = GetStringProperty(value, "webSocketDebuggerUrl");

  if (base::StartsWith(target_path, "ws://", base::CompareCase::SENSITIVE)) {
    size_t pos = target_path.find("/", 5);
    if (pos == std::string::npos) {
      pos = 5;
    }
    target_path = target_path.substr(pos);
  } else {
    target_path = std::string();
  }
  return target_path;
}

// static
scoped_refptr<content::CitizenNotesAgentHost>
AgentHostDelegate::GetOrCreateAgentHost(
    scoped_refptr<CNAndroidDeviceManager::Device> device,
    const std::string& browser_id,
    const std::string& browser_version,
    const std::string& local_id,
    const std::string& target_path,
    const std::string& type,
    const base::Value::Dict* value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<CitizenNotesAgentHost> result =
      CitizenNotesAgentHost::GetForId(local_id);
  if (result) {
    return result;
  }

  AgentHostDelegate* delegate = new AgentHostDelegate(
      device, browser_id, browser_version, local_id, target_path, type, value);
  result =
      content::CitizenNotesAgentHost::Forward(local_id, base::WrapUnique(delegate));
  delegate->agent_host_ = result.get();
  return result;
}

AgentHostDelegate::AgentHostDelegate(
    scoped_refptr<CNAndroidDeviceManager::Device> device,
    const std::string& browser_id,
    const std::string& browser_version,
    const std::string& local_id,
    const std::string& target_path,
    const std::string& type,
    const base::Value::Dict* value)
    : device_(device),
      browser_id_(browser_id),
      local_id_(local_id),
      target_path_(target_path),
      remote_type_(type),
      remote_id_(value ? GetStringProperty(*value, "id") : ""),
      frontend_url_(value ? GetFrontendURLFromValue(*value, browser_version)
                          : ""),
      title_(value ? base::UTF16ToUTF8(base::UnescapeForHTML(
                         base::UTF8ToUTF16(GetStringProperty(*value, "title"))))
                   : ""),
      description_(value ? GetStringProperty(*value, "description") : ""),
      url_(GURL(value ? GetStringProperty(*value, "url") : "")),
      favicon_url_(GURL(value ? GetStringProperty(*value, "faviconUrl") : "")),
      agent_host_(nullptr) {}

AgentHostDelegate::~AgentHostDelegate() = default;

void AgentHostDelegate::Attach(content::CitizenNotesExternalAgentProxy* proxy) {
  std::unique_ptr<WebSocketProxy> ws_proxy(new WebSocketProxy(proxy));
  ws_proxy->WebSocketCreated(
      device_->CreateWebSocket(browser_id_, target_path_, ws_proxy.get()));
  proxies_[proxy] = std::move(ws_proxy);
  base::RecordAction(
      base::StartsWith(browser_id_, kWebViewSocketPrefix,
                       base::CompareCase::SENSITIVE)
          ? base::UserMetricsAction("CitizenNotes_InspectAndroidWebView")
          : base::UserMetricsAction("CitizenNotes_InspectAndroidPage"));
}

void AgentHostDelegate::Detach(content::CitizenNotesExternalAgentProxy* proxy) {
  proxies_.erase(proxy);
}

std::string AgentHostDelegate::GetType() {
  return remote_type_;
}

std::string AgentHostDelegate::GetTitle() {
  return title_;
}

std::string AgentHostDelegate::GetDescription() {
  return description_;
}

GURL AgentHostDelegate::GetURL() {
  return url_;
}

GURL AgentHostDelegate::GetFaviconURL() {
  return favicon_url_;
}

std::string AgentHostDelegate::GetFrontendURL() {
  return frontend_url_;
}

bool AgentHostDelegate::Activate() {
  std::string request =
      base::StringPrintf(kActivatePageRequest, remote_id_.c_str());
  device_->SendJsonRequest(browser_id_, request, base::DoNothing());
  return true;
}

void AgentHostDelegate::Reload() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (target_path_.empty()) {
    return;
  }
  new ProtocolCommand(device_, browser_id_, target_path_, kPageReloadCommand);
}

bool AgentHostDelegate::Close() {
  std::string request =
      base::StringPrintf(kClosePageRequest, remote_id_.c_str());
  device_->SendJsonRequest(browser_id_, request, base::DoNothing());
  return true;
}

base::TimeTicks AgentHostDelegate::GetLastActivityTime() {
  return base::TimeTicks();
}

void AgentHostDelegate::SendMessageToBackend(
    content::CitizenNotesExternalAgentProxy* proxy,
    base::span<const uint8_t> message) {
  auto it = proxies_.find(proxy);
  // We could have detached due to physical connection being closed.
  if (it == proxies_.end()) {
    return;
  }
  it->second->SendMessageToBackend(std::string(message.begin(), message.end()));
}

}  // namespace

// CitizenNotesDeviceDiscovery::DiscoveryRequest ----------------------------------

class CitizenNotesDeviceDiscovery::DiscoveryRequest
    : public base::RefCountedThreadSafe<DiscoveryRequest,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  static void Start(CNAndroidDeviceManager* device_manager,
                    base::OnceCallback<void(const CompleteDevices&)> callback);

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<DiscoveryRequest>;
  explicit DiscoveryRequest(
      base::OnceCallback<void(const CompleteDevices&)> callback);
  virtual ~DiscoveryRequest();

  void ReceivedDevices(const CNAndroidDeviceManager::Devices& devices);
  void ReceivedDeviceInfo(scoped_refptr<CNAndroidDeviceManager::Device> device,
                          const CNAndroidDeviceManager::DeviceInfo& device_info);
  void ReceivedVersion(scoped_refptr<CNAndroidDeviceManager::Device> device,
                       scoped_refptr<RemoteBrowser>,
                       int result,
                       const std::string& response);
  void ReceivedPages(scoped_refptr<CNAndroidDeviceManager::Device> device,
                     scoped_refptr<RemoteBrowser>,
                     int result,
                     const std::string& response);
  void ParseBrowserInfo(scoped_refptr<RemoteBrowser> browser,
                        const std::string& version_response,
                        bool& is_chrome);

  base::OnceCallback<void(const CompleteDevices&)> callback_;
  CitizenNotesDeviceDiscovery::CompleteDevices complete_devices_;
};

// static
void CitizenNotesDeviceDiscovery::DiscoveryRequest::Start(
    CNAndroidDeviceManager* device_manager,
    base::OnceCallback<void(const CompleteDevices&)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto request =
      base::WrapRefCounted(new DiscoveryRequest(std::move(callback)));
  device_manager->QueryDevices(
      base::BindOnce(&DiscoveryRequest::ReceivedDevices, request));
}

CitizenNotesDeviceDiscovery::DiscoveryRequest::DiscoveryRequest(
    base::OnceCallback<void(const CompleteDevices&)> callback)
    : callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CitizenNotesDeviceDiscovery::DiscoveryRequest::~DiscoveryRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback_).Run(complete_devices_);
}

void CitizenNotesDeviceDiscovery::DiscoveryRequest::ReceivedDevices(
    const CNAndroidDeviceManager::Devices& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& device : devices) {
    device->QueryDeviceInfo(
        base::BindOnce(&DiscoveryRequest::ReceivedDeviceInfo, this, device));
  }
}

void CitizenNotesDeviceDiscovery::DiscoveryRequest::ReceivedDeviceInfo(
    scoped_refptr<CNAndroidDeviceManager::Device> device,
    const CNAndroidDeviceManager::DeviceInfo& device_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<RemoteDevice> remote_device =
      new RemoteDevice(device->serial(), device_info);
  complete_devices_.push_back(std::make_pair(device, remote_device));
  for (auto it = remote_device->browsers().begin();
       it != remote_device->browsers().end(); ++it) {
    device->SendJsonRequest(
        (*it)->socket(), kVersionRequest,
        base::BindOnce(&DiscoveryRequest::ReceivedVersion, this, device, *it));
  }
}

void CitizenNotesDeviceDiscovery::DiscoveryRequest::ParseBrowserInfo(
    scoped_refptr<RemoteBrowser> browser,
    const std::string& version_response,
    bool& is_chrome) {
  // Parse version, append to package name if available,
  absl::optional<base::Value::Dict> value_dict =
      base::JSONReader::ReadDict(version_response);
  if (!value_dict) {
    return;
  }
  const std::string* browser_name = value_dict->FindString("Browser");
  if (browser_name) {
    std::vector<base::StringPiece> parts = base::SplitStringPiece(
        *browser_name, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.size() == 2) {
      browser->version_ = parts[1];
      is_chrome = parts[0] == "Chrome" || parts[0] == "HeadlessChrome";
    } else {
      browser->version_ = *browser_name;
    }
  }
  browser->browser_target_id_ = GetTargetPath(*value_dict);
  if (browser->browser_target_id_.empty()) {
    browser->browser_target_id_ = kBrowserTargetSocket;
  }
  const std::string* package = value_dict->FindString("Android-Package");
  if (package) {
    browser->display_name_ =
        CNAndroidDeviceManager::GetBrowserName(browser->socket(), *package);
  }
}

void CitizenNotesDeviceDiscovery::DiscoveryRequest::ReceivedVersion(
    scoped_refptr<CNAndroidDeviceManager::Device> device,
    scoped_refptr<RemoteBrowser> browser,
    int result,
    const std::string& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::string url = kPageListRequest;
  bool is_chrome = false;
  if (result >= 0) {
    ParseBrowserInfo(browser, response, is_chrome);
  }
  if (base::FeatureList::IsEnabled(::features::kCitizenNotesTabTarget) &&
      is_chrome) {
    url += "?for_tab";
  }
  device->SendJsonRequest(
      browser->socket(), url,
      base::BindOnce(&DiscoveryRequest::ReceivedPages, this, device, browser));
}

void CitizenNotesDeviceDiscovery::DiscoveryRequest::ReceivedPages(
    scoped_refptr<CNAndroidDeviceManager::Device> device,
    scoped_refptr<RemoteBrowser> browser,
    int result,
    const std::string& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (result < 0) {
    return;
  }
  absl::optional<base::Value> value = base::JSONReader::Read(response);
  if (!value) {
    return;
  }
  auto* value_list = value->GetIfList();
  if (!value_list) {
    return;
  }
  for (base::Value& page_value : *value_list) {
    if (page_value.is_dict()) {
      browser->pages_.push_back(
          new RemotePage(device, browser->browser_id_, browser->version_,
                         std::move(page_value).TakeDict()));
    }
  }
}

// CitizenNotesDeviceDiscovery::RemotePage ----------------------------------------

CitizenNotesDeviceDiscovery::RemotePage::RemotePage(
    scoped_refptr<CNAndroidDeviceManager::Device> device,
    const std::string& browser_id,
    const std::string& browser_version,
    base::Value::Dict dict)
    : device_(device),
      browser_id_(browser_id),
      browser_version_(browser_version),
      dict_(std::move(dict)) {}

CitizenNotesDeviceDiscovery::RemotePage::~RemotePage() {}

scoped_refptr<content::CitizenNotesAgentHost>
CitizenNotesDeviceDiscovery::RemotePage::CreateTarget() {
  std::string local_id =
      BuildUniqueTargetId(device_->serial(), browser_id_, dict_);
  std::string target_path = GetTargetPath(dict_);
  std::string type = GetStringProperty(dict_, "type");

  std::string port_num = browser_id_;
  if (type == "node") {
    port_num = GURL(GetStringProperty(dict_, "webSocketDebuggerUrl")).port();
  }

  agent_host_ = AgentHostDelegate::GetOrCreateAgentHost(
      device_, port_num, browser_version_, local_id, target_path, type, &dict_);
  return agent_host_;
}

// CitizenNotesDeviceDiscovery::RemoteBrowser -------------------------------------

CitizenNotesDeviceDiscovery::RemoteBrowser::RemoteBrowser(
    const std::string& serial,
    const CNAndroidDeviceManager::BrowserInfo& browser_info)
    : serial_(serial),
      browser_id_(browser_info.socket_name),
      display_name_(browser_info.display_name),
      user_(browser_info.user),
      type_(browser_info.type) {}

bool CitizenNotesDeviceDiscovery::RemoteBrowser::IsChrome() {
  return type_ == CNAndroidDeviceManager::BrowserInfo::kTypeChrome;
}

std::string CitizenNotesDeviceDiscovery::RemoteBrowser::GetId() {
  return serial() + ":" + socket();
}

CitizenNotesDeviceDiscovery::RemoteBrowser::ParsedVersion
CitizenNotesDeviceDiscovery::RemoteBrowser::GetParsedVersion() {
  ParsedVersion result;
  for (const base::StringPiece& part : base::SplitStringPiece(
           version_, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    int value = 0;
    base::StringToInt(part, &value);
    result.push_back(value);
  }
  return result;
}

CitizenNotesDeviceDiscovery::RemoteBrowser::~RemoteBrowser() {}

// CitizenNotesDeviceDiscovery::RemoteDevice --------------------------------------

CitizenNotesDeviceDiscovery::RemoteDevice::RemoteDevice(
    const std::string& serial,
    const CNAndroidDeviceManager::DeviceInfo& device_info)
    : serial_(serial),
      model_(device_info.model),
      connected_(device_info.connected),
      screen_size_(device_info.screen_size) {
  for (auto it = device_info.browser_info.begin();
       it != device_info.browser_info.end(); ++it) {
    browsers_.push_back(new RemoteBrowser(serial, *it));
  }
}

CitizenNotesDeviceDiscovery::RemoteDevice::~RemoteDevice() {}

// CitizenNotesDeviceDiscovery ----------------------------------------------------

CitizenNotesDeviceDiscovery::CitizenNotesDeviceDiscovery(
    CNAndroidDeviceManager* device_manager,
    DeviceListCallback callback)
    : device_manager_(device_manager),
      callback_(std::move(callback)),
      task_scheduler_(base::BindRepeating(&ScheduleTaskDefault)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RequestDeviceList();
}

CitizenNotesDeviceDiscovery::~CitizenNotesDeviceDiscovery() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void CitizenNotesDeviceDiscovery::SetScheduler(
    base::RepeatingCallback<void(base::OnceClosure)> scheduler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  task_scheduler_ = std::move(scheduler);
}

// static
scoped_refptr<content::CitizenNotesAgentHost>
CitizenNotesDeviceDiscovery::CreateBrowserAgentHost(
    scoped_refptr<CNAndroidDeviceManager::Device> device,
    scoped_refptr<RemoteBrowser> browser) {
  return AgentHostDelegate::GetOrCreateAgentHost(
      device, browser->browser_id_, browser->version_,
      "adb:" + browser->serial() + ":" + browser->socket(),
      browser->browser_target_id(), CitizenNotesAgentHost::kTypeBrowser, nullptr);
}

void CitizenNotesDeviceDiscovery::RequestDeviceList() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DiscoveryRequest::Start(
      device_manager_,
      base::BindOnce(&CitizenNotesDeviceDiscovery::ReceivedDeviceList,
                     weak_factory_.GetWeakPtr()));
}

void CitizenNotesDeviceDiscovery::ReceivedDeviceList(
    const CompleteDevices& complete_devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  task_scheduler_.Run(base::BindOnce(
      &CitizenNotesDeviceDiscovery::RequestDeviceList, weak_factory_.GetWeakPtr()));
  // |callback_| should be run last as it may destroy |this|.
  callback_.Run(complete_devices);
}
