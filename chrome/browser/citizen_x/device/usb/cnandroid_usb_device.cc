// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/citizen_x/device/usb/cnandroid_usb_device.h"

#include <set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/citizen_x/device/usb/cnandroid_rsa.h"
#include "chrome/browser/citizen_x/device/usb/cnandroid_usb_socket.h"
#include "crypto/rsa_private_key.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

using device::mojom::UsbTransferStatus;

namespace {

const size_t kHeaderSize = 24;

const int kUsbTimeout = 0;

const uint32_t kMaxPayload = 4096;
const uint32_t kVersion = 0x01000000;

static const char kHostConnectMessage[] = "host::";

// Stores android wrappers around claimed usb devices on caller thread.
base::LazyInstance<std::vector<CNAndroidUsbDevice*>>::Leaky g_devices =
    LAZY_INSTANCE_INITIALIZER;

// Stores the GUIDs of devices that are currently opened so that they are not
// re-probed.
base::LazyInstance<std::vector<std::string>>::Leaky g_open_devices =
    LAZY_INSTANCE_INITIALIZER;

uint32_t Checksum(const std::string& data) {
  unsigned char* x = (unsigned char*)data.data();
  int count = data.length();
  uint32_t sum = 0;
  while (count-- > 0)
    sum += *x++;
  return sum;
}

void DumpMessage(bool outgoing, const uint8_t* data, size_t length) {
#if 0
  std::string result;
  if (length == kHeaderSize) {
    for (size_t i = 0; i < 24; ++i) {
      result += base::StringPrintf("%02x", data[i]);
      if ((i + 1) % 4 == 0)
        result += " ";
    }
    for (size_t i = 0; i < 24; ++i) {
      if (data[i] >= 0x20 && data[i] <= 0x7E)
        result += data[i];
      else
        result += ".";
    }
  } else {
    result = base::StringPrintf("%d: ", static_cast<int>(length));
    for (size_t i = 0; i < length; ++i) {
      if (data[i] >= 0x20 && data[i] <= 0x7E)
        result += data[i];
      else
        result += ".";
    }
  }
  LOG(ERROR) << (outgoing ? "[out] " : "[ in] ") << result;
#endif  // 0
}

void OnProbeFinished(CNAndroidUsbDevicesCallback callback,
                     CNAndroidUsbDevices* new_devices) {
  std::unique_ptr<CNAndroidUsbDevices> devices(new_devices);

  // Add raw pointers to the newly claimed devices.
  for (const scoped_refptr<CNAndroidUsbDevice>& device : *devices) {
    g_devices.Get().push_back(device.get());
  }

  // Return all claimed devices.
  CNAndroidUsbDevices result(g_devices.Get().begin(), g_devices.Get().end());
  std::move(callback).Run(result);
}

void OnDeviceClosed(const std::string& guid,
                    mojo::Remote<device::mojom::UsbDevice> device) {
  base::Erase(g_open_devices.Get(), guid);
}

void OnDeviceClosedWithBarrier(const std::string& guid,
                               mojo::Remote<device::mojom::UsbDevice> device,
                               const base::RepeatingClosure& barrier) {
  base::Erase(g_open_devices.Get(), guid);
  barrier.Run();
}

void CreateDeviceOnInterfaceClaimed(
    CNAndroidUsbDevices* devices,
    crypto::RSAPrivateKey* rsa_key,
    CNAndroidDeviceInfo android_device_info,
    mojo::Remote<device::mojom::UsbDevice> device,
    const base::RepeatingClosure& barrier,
    device::mojom::UsbClaimInterfaceResult result) {
  if (result == device::mojom::UsbClaimInterfaceResult::kSuccess) {
    devices->push_back(
        new CNAndroidUsbDevice(rsa_key, android_device_info, std::move(device)));
    barrier.Run();
  } else {
    auto* device_raw = device.get();
    device_raw->Close(base::BindOnce(&OnDeviceClosedWithBarrier,
                                     android_device_info.guid,
                                     std::move(device), barrier));
  }
}

void OnInterfaceReleased(mojo::Remote<device::mojom::UsbDevice> device,
                         const std::string& guid,
                         bool release_successful) {
  auto* device_raw = device.get();
  device_raw->Close(base::BindOnce(&OnDeviceClosed, guid, std::move(device)));
}

void OnDeviceOpened(CNAndroidUsbDevices* devices,
                    crypto::RSAPrivateKey* rsa_key,
                    CNAndroidDeviceInfo android_device_info,
                    mojo::Remote<device::mojom::UsbDevice> device,
                    const base::RepeatingClosure& barrier,
                    device::mojom::UsbOpenDeviceResultPtr result) {
  // If the error is UsbOpenDeviceError::ALREADY_OPEN we all try to claim the
  // interface because the device may be opened by other modules or extensions
  // for different interface.
  if (result->is_success() ||
      result->get_error() == device::mojom::UsbOpenDeviceError::ALREADY_OPEN) {
    DCHECK(device);
    auto* device_raw = device.get();
    device_raw->ClaimInterface(
        android_device_info.interface_id,
        base::BindOnce(&CreateDeviceOnInterfaceClaimed, devices, rsa_key,
                       android_device_info, std::move(device), barrier));
  } else {
    base::Erase(g_open_devices.Get(), android_device_info.guid);
    barrier.Run();
  }
}

void OpenAndroidDevices(crypto::RSAPrivateKey* rsa_key,
                        CNAndroidUsbDevicesCallback callback,
                        std::vector<CNAndroidDeviceInfo> device_info_list) {
  // Add new devices.
  CNAndroidUsbDevices* devices = new CNAndroidUsbDevices();
  base::RepeatingClosure barrier = base::BarrierClosure(
      device_info_list.size(),
      base::BindOnce(&OnProbeFinished, std::move(callback), devices));

  for (const auto& device_info : device_info_list) {
    if (base::Contains(g_open_devices.Get(), device_info.guid)) {
      // This device is already open, do not make parallel attempts to connect
      // to it.
      barrier.Run();
      continue;
    }
    g_open_devices.Get().push_back(device_info.guid);

    mojo::Remote<device::mojom::UsbDevice> device;
    CNUsbDeviceManagerHelper::GetInstance()->GetDevice(
        device_info.guid, device.BindNewPipeAndPassReceiver());
    auto* device_raw = device.get();
    device_raw->Open(base::BindOnce(&OnDeviceOpened, devices, rsa_key,
                                    device_info, std::move(device), barrier));
  }
}

}  // namespace

CNAdbMessage::CNAdbMessage(uint32_t command,
                       uint32_t arg0,
                       uint32_t arg1,
                       const std::string& body)
    : command(command), arg0(arg0), arg1(arg1), body(body) {}

CNAdbMessage::~CNAdbMessage() {}

// static
void CNAndroidUsbDevice::Enumerate(crypto::RSAPrivateKey* rsa_key,
                                 CNAndroidUsbDevicesCallback callback) {
  CNUsbDeviceManagerHelper::GetInstance()->GetAndroidDevices(
      base::BindOnce(&OpenAndroidDevices, rsa_key, std::move(callback)));
}

CNAndroidUsbDevice::CNAndroidUsbDevice(
    crypto::RSAPrivateKey* rsa_key,
    const CNAndroidDeviceInfo& android_device_info,
    mojo::Remote<device::mojom::UsbDevice> device)
    : rsa_key_(rsa_key->Copy()),
      device_(std::move(device)),
      android_device_info_(android_device_info),
      is_connected_(false),
      signature_sent_(false),
      last_socket_id_(256) {
  DCHECK(device_);
  device_.set_disconnect_handler(
      base::BindOnce(&CNAndroidUsbDevice::Terminate, weak_factory_.GetWeakPtr()));
}

void CNAndroidUsbDevice::InitOnCallerThread() {
  if (task_runner_)
    return;
  task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  Queue(std::make_unique<CNAdbMessage>(CNAdbMessage::kCommandCNXN, kVersion,
                                     kMaxPayload, kHostConnectMessage));
  ReadHeader();
}

net::StreamSocket* CNAndroidUsbDevice::CreateSocket(const std::string& command) {
  if (!device_)
    return nullptr;

  uint32_t socket_id = ++last_socket_id_;
  sockets_[socket_id] = new CNAndroidUsbSocket(
      this, socket_id, command,
      base::BindOnce(&CNAndroidUsbDevice::SocketDeleted, this, socket_id));
  return sockets_[socket_id];
}

void CNAndroidUsbDevice::Send(uint32_t command,
                            uint32_t arg0,
                            uint32_t arg1,
                            const std::string& body) {
  auto message = std::make_unique<CNAdbMessage>(command, arg0, arg1, body);
  // Delay open request if not yet connected.
  if (!is_connected_) {
    pending_messages_.push_back(std::move(message));
    return;
  }
  Queue(std::move(message));
}

CNAndroidUsbDevice::~CNAndroidUsbDevice() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  Terminate();
}

void CNAndroidUsbDevice::Queue(std::unique_ptr<CNAdbMessage> message) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Queue header.
  std::vector<uint32_t> header;
  header.push_back(message->command);
  header.push_back(message->arg0);
  header.push_back(message->arg1);
  bool append_zero = true;
  if (message->body.empty())
    append_zero = false;
  if (message->command == CNAdbMessage::kCommandAUTH &&
      message->arg0 == CNAdbMessage::kAuthSignature)
    append_zero = false;
  if (message->command == CNAdbMessage::kCommandWRTE)
    append_zero = false;

  size_t body_length = message->body.length() + (append_zero ? 1 : 0);
  header.push_back(body_length);
  header.push_back(Checksum(message->body));
  header.push_back(message->command ^ 0xffffffff);
  // TODO(donna.wu@intel.com): eliminate the buffer copy here, needs to change
  // type BulkMessage.
  auto header_buffer = base::MakeRefCounted<base::RefCountedBytes>(
      reinterpret_cast<uint8_t*>(header.data()), kHeaderSize);
  outgoing_queue_.push(header_buffer);

  // Queue body.
  if (!message->body.empty()) {
    auto body_buffer = base::MakeRefCounted<base::RefCountedBytes>(body_length);
    memcpy(body_buffer->front(), message->body.data(), message->body.length());
    if (append_zero)
      body_buffer->data()[body_length - 1] = 0;
    outgoing_queue_.push(body_buffer);
    if (android_device_info_.zero_mask &&
        (body_length & android_device_info_.zero_mask) == 0) {
      // Send a zero length packet.
      outgoing_queue_.push(base::MakeRefCounted<base::RefCountedBytes>(0));
    }
  }
  ProcessOutgoing();
}

void CNAndroidUsbDevice::ProcessOutgoing() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (outgoing_queue_.empty() || !device_)
    return;

  BulkMessage message = outgoing_queue_.front();
  outgoing_queue_.pop();
  DumpMessage(true, message->front(), message->size());

  device_->GenericTransferOut(
      android_device_info_.outbound_address, message->data(), kUsbTimeout,
      base::BindOnce(&CNAndroidUsbDevice::OutgoingMessageSent,
                     weak_factory_.GetWeakPtr()));
}

void CNAndroidUsbDevice::OutgoingMessageSent(UsbTransferStatus status) {
  if (status != UsbTransferStatus::COMPLETED)
    return;

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CNAndroidUsbDevice::ProcessOutgoing, this));
}

void CNAndroidUsbDevice::ReadHeader() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!device_)
    return;

  device_->GenericTransferIn(android_device_info_.inbound_address, kHeaderSize,
                             kUsbTimeout,
                             base::BindOnce(&CNAndroidUsbDevice::ParseHeader,
                                            weak_factory_.GetWeakPtr()));
}

void CNAndroidUsbDevice::ParseHeader(UsbTransferStatus status,
                                   base::span<const uint8_t> buffer) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (status == UsbTransferStatus::TIMEOUT) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&CNAndroidUsbDevice::ReadHeader, this));
    return;
  }

  if (status != UsbTransferStatus::COMPLETED || buffer.size() != kHeaderSize) {
    TransferError(status);
    return;
  }

  DumpMessage(false, buffer.data(), buffer.size());
  const auto* header = reinterpret_cast<const uint32_t*>(buffer.data());
  std::unique_ptr<CNAdbMessage> message(
      new CNAdbMessage(header[0], header[1], header[2], ""));
  uint32_t data_length = header[3];
  uint32_t data_check = header[4];
  uint32_t magic = header[5];
  if ((message->command ^ 0xffffffff) != magic) {
    TransferError(UsbTransferStatus::TRANSFER_ERROR);
    return;
  }

  if (data_length == 0) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CNAndroidUsbDevice::HandleIncoming, this,
                                  std::move(message)));
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CNAndroidUsbDevice::ReadBody, this,
                                  std::move(message), data_length, data_check));
  }
}

void CNAndroidUsbDevice::ReadBody(std::unique_ptr<CNAdbMessage> message,
                                uint32_t data_length,
                                uint32_t data_check) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!device_.get()) {
    return;
  }

  device_->GenericTransferIn(
      android_device_info_.inbound_address, data_length, kUsbTimeout,
      base::BindOnce(&CNAndroidUsbDevice::ParseBody, weak_factory_.GetWeakPtr(),
                     std::move(message), data_length, data_check));
}

void CNAndroidUsbDevice::ParseBody(std::unique_ptr<CNAdbMessage> message,
                                 uint32_t data_length,
                                 uint32_t data_check,
                                 UsbTransferStatus status,
                                 base::span<const uint8_t> buffer) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (status == UsbTransferStatus::TIMEOUT) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CNAndroidUsbDevice::ReadBody, this,
                                  std::move(message), data_length, data_check));
    return;
  }

  if (status != UsbTransferStatus::COMPLETED ||
      static_cast<uint32_t>(buffer.size()) != data_length) {
    TransferError(status);
    return;
  }

  DumpMessage(false, buffer.data(), data_length);
  message->body =
      std::string(reinterpret_cast<const char*>(buffer.data()), buffer.size());
  if (Checksum(message->body) != data_check) {
    TransferError(UsbTransferStatus::TRANSFER_ERROR);
    return;
  }

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&CNAndroidUsbDevice::HandleIncoming, this,
                                        std::move(message)));
}

void CNAndroidUsbDevice::HandleIncoming(std::unique_ptr<CNAdbMessage> message) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  switch (message->command) {
    case CNAdbMessage::kCommandAUTH: {
      DCHECK_EQ(message->arg0, static_cast<uint32_t>(CNAdbMessage::kAuthToken));
      if (signature_sent_) {
        Queue(std::make_unique<CNAdbMessage>(
            CNAdbMessage::kCommandAUTH, CNAdbMessage::kAuthRSAPublicKey, 0,
            CNAndroidRSAPublicKey(rsa_key_.get())));
      } else {
        signature_sent_ = true;
        std::string signature = CNAndroidRSASign(rsa_key_.get(), message->body);
        if (!signature.empty()) {
          Queue(std::make_unique<CNAdbMessage>(CNAdbMessage::kCommandAUTH,
                                             CNAdbMessage::kAuthSignature, 0,
                                             signature));
        } else {
          Queue(std::make_unique<CNAdbMessage>(
              CNAdbMessage::kCommandAUTH, CNAdbMessage::kAuthRSAPublicKey, 0,
              CNAndroidRSAPublicKey(rsa_key_.get())));
        }
      }
    } break;
    case CNAdbMessage::kCommandCNXN:
      {
        is_connected_ = true;
        PendingMessages pending;
        pending.swap(pending_messages_);
        for (auto& msg : pending)
          Queue(std::move(msg));
      }
      break;
    case CNAdbMessage::kCommandOKAY:
    case CNAdbMessage::kCommandWRTE:
    case CNAdbMessage::kCommandCLSE:
      {
      auto it = sockets_.find(message->arg1);
      if (it != sockets_.end())
        it->second->HandleIncoming(std::move(message));
      }
      break;
    default:
      break;
  }
  ReadHeader();
}

void CNAndroidUsbDevice::TransferError(UsbTransferStatus status) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  Terminate();
}

void CNAndroidUsbDevice::Terminate() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Remove this CNAndroidUsbDevice from |g_devices|.
  auto it = base::ranges::find(g_devices.Get(), this);
  if (it != g_devices.Get().end())
    g_devices.Get().erase(it);

  // For connection error, remove the guid from recored opening/opened list.
  // For transfer errors, we'll do this after releasing the interface.
  if (!device_) {
    base::Erase(g_open_devices.Get(), android_device_info_.guid);
    return;
  }

  // For Transfer error case.
  // Make sure we zero-out |device_| so that closing connections did not
  // open new socket connections.
  mojo::Remote<device::mojom::UsbDevice> device = std::move(device_);
  device_.reset();

  // Iterate over copy.
  CNAndroidUsbSockets sockets(sockets_);
  for (auto socket_it = sockets.begin(); socket_it != sockets.end();
       ++socket_it) {
    socket_it->second->Terminated(true);
  }
  DCHECK(sockets_.empty());

  auto* device_raw = device.get();
  device_raw->ReleaseInterface(
      android_device_info_.interface_id,
      base::BindOnce(&OnInterfaceReleased, std::move(device),
                     android_device_info_.guid));
}

void CNAndroidUsbDevice::SocketDeleted(uint32_t socket_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  sockets_.erase(socket_id);
}
