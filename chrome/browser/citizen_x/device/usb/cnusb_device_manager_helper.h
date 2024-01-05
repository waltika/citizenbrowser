// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_DEVICE_USB_USB_DEVICE_MANAGER_HELPER_H_
#define CHROME_BROWSER_CITIZENNOTES_DEVICE_USB_USB_DEVICE_MANAGER_HELPER_H_

#include <string>
#include <vector>

#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"

struct CNAndroidInterfaceInfo {
  CNAndroidInterfaceInfo(
      uint8_t interface_number,
      const device::mojom::UsbAlternateInterfaceInfo* alternate);

  uint8_t interface_number;
  RAW_PTR_EXCLUSION const device::mojom::UsbAlternateInterfaceInfo* alternate;
};

struct CNAndroidDeviceInfo {
  CNAndroidDeviceInfo(const std::string& guid,
                    const std::string& serial,
                    int interface_id,
                    int inbound_address,
                    int outbound_address,
                    int zero_mask);
  CNAndroidDeviceInfo(const CNAndroidDeviceInfo& other);

  std::string guid;
  std::string serial;
  int interface_id = 0;
  int inbound_address = 0;
  int outbound_address = 0;
  int zero_mask = 0;
};

using CNAndroidDeviceInfoListCallback =
    base::OnceCallback<void(std::vector<CNAndroidDeviceInfo>)>;

// All methods in this class should be called in the single thread,
// handler_thread_ in AndroidDeviceManager.
class CNUsbDeviceManagerHelper {
 public:
  // This is prefered way to get CNUsbDeviceManagerHelper instance.
  static CNUsbDeviceManagerHelper* GetInstance();
  static void CountDevices(base::OnceCallback<void(int)> callback);
  static void SetUsbManagerForTesting(
      mojo::PendingRemote<device::mojom::UsbDeviceManager> fake_usb_manager);

  // Please do not create CNUsbDeviceManagerHelper instance from this constructor
  // directly, use static method GetInstance() instead.
  CNUsbDeviceManagerHelper();

  CNUsbDeviceManagerHelper(const CNUsbDeviceManagerHelper&) = delete;
  CNUsbDeviceManagerHelper& operator=(const CNUsbDeviceManagerHelper&) = delete;

  virtual ~CNUsbDeviceManagerHelper();

  void GetAndroidDevices(CNAndroidDeviceInfoListCallback callback);

  void GetDevice(
      const std::string& guid,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver);

 private:
  void CountDevicesInternal(base::OnceCallback<void(int)> callback);
  void SetUsbManagerForTestingInternal(
      mojo::PendingRemote<device::mojom::UsbDeviceManager> fake_usb_manager);
  void EnsureUsbDeviceManagerConnection();
  void OnDeviceManagerConnectionError();

  mojo::Remote<device::mojom::UsbDeviceManager> device_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Just for test.
  mojo::PendingRemote<device::mojom::UsbDeviceManager> testing_device_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CNUsbDeviceManagerHelper> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CITIZENNOTES_DEVICE_USB_USB_DEVICE_MANAGER_HELPER_H_
