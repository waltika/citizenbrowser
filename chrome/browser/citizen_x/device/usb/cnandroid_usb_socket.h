// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_DEVICE_USB_ANDROID_USB_SOCKET_H_
#define CHROME_BROWSER_CITIZENNOTES_DEVICE_USB_ANDROID_USB_SOCKET_H_

#include <stdint.h>

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/citizen_x/device/usb/cnandroid_usb_device.h"
#include "net/base/ip_endpoint.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

class CNAndroidUsbSocket : public net::StreamSocket {
 public:
  CNAndroidUsbSocket(scoped_refptr<CNAndroidUsbDevice> device,
                   uint32_t socket_id,
                   const std::string& command,
                   base::OnceClosure delete_callback);

  CNAndroidUsbSocket(const CNAndroidUsbSocket&) = delete;
  CNAndroidUsbSocket& operator=(const CNAndroidUsbSocket&) = delete;

  ~CNAndroidUsbSocket() override;

  void HandleIncoming(std::unique_ptr<CNAdbMessage> message);

  void Terminated(bool closed_by_device);

  // net::StreamSocket implementation.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  int Write(
      net::IOBuffer* buf,
      int buf_len,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  int Connect(net::CompletionOnceCallback callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(net::IPEndPoint* address) const override;
  int GetLocalAddress(net::IPEndPoint* address) const override;
  const net::NetLogWithSource& NetLog() const override;
  bool WasEverUsed() const override;
  net::NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(net::SSLInfo* ssl_info) override;
  int64_t GetTotalReceivedBytes() const override;
  void ApplySocketTag(const net::SocketTag& tag) override;

 private:
  void RespondToReader(bool disconnect);
  void RespondToWriter(int result);

  scoped_refptr<CNAndroidUsbDevice> device_;
  std::string command_;
  uint32_t local_id_;
  uint32_t remote_id_;
  net::NetLogWithSource net_log_;
  bool is_connected_;
  std::string read_buffer_;
  scoped_refptr<net::IOBuffer> read_io_buffer_;
  int read_length_;
  int write_length_;
  net::CompletionOnceCallback connect_callback_;
  net::CompletionOnceCallback read_callback_;
  net::CompletionOnceCallback write_callback_;
  base::OnceClosure delete_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CNAndroidUsbSocket> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CITIZENNOTES_DEVICE_USB_ANDROID_USB_SOCKET_H_
