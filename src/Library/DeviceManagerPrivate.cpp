//
// Copyright (C) 2015 Red Hat, Inc.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Authors: Daniel Kopecek <dkopecek@redhat.com>
//
#include "DeviceManagerPrivate.hpp"
#include <DeviceManagerHooks.hpp>
#include "Exception.hpp"
#include "Logger.hpp"

namespace usbguard {
  DeviceManagerPrivate::DeviceManagerPrivate(DeviceManager& p_instance, DeviceManagerHooks& hooks)
    : _p_instance(p_instance),
      _hooks(hooks)
  {
  }

  DeviceManagerPrivate::DeviceManagerPrivate(DeviceManager& p_instance, const DeviceManagerPrivate& rhs)
    : _p_instance(p_instance),
      _hooks(rhs._hooks)
  {
    *this = rhs;
  }

  const DeviceManagerPrivate& DeviceManagerPrivate::operator=(const DeviceManagerPrivate& rhs)
  {
    std::unique_lock<std::mutex> local_device_map_lock(_device_map_mutex);
    std::unique_lock<std::mutex> remote_device_map_lock(rhs._device_map_mutex);
    _device_map = rhs._device_map;
    return *this;
  }

  void DeviceManagerPrivate::setRestoreControllerDeviceState(bool enabled)
  {
    _restore_controller_device_state = enabled;
  }

  bool DeviceManagerPrivate::getRestoreControllerDeviceState() const
  {
    return _restore_controller_device_state;
  }

  void DeviceManagerPrivate::insertDevice(Pointer<Device> device)
  {
    USBGUARD_LOG(Trace) << "device_ptr=" << device.get();
    std::unique_lock<std::mutex> device_map_lock(_device_map_mutex);
    const uint32_t id = _hooks.dmHookAssignID();
    USBGUARD_LOG(Debug) << "id=" << id;
    device->setID(id);
    _device_map[id] = device;
  }

  Pointer<Device> DeviceManagerPrivate::removeDevice(uint32_t id)
  {
    USBGUARD_LOG(Trace) << "entry: id=" << id;
    std::unique_lock<std::mutex> device_map_lock(_device_map_mutex);
    auto it = _device_map.find(id);
    if (it == _device_map.end()) {
      throw Exception("Device remove", "device id", "id doesn't exist");
    }
    Pointer<Device> device = it->second;
    _device_map.erase(it);
    USBGUARD_LOG(Trace) << "return: device_ptr=" << device.get();
    return device;
  }

  PointerVector<Device> DeviceManagerPrivate::getDeviceList()
  {
    std::unique_lock<std::mutex> device_map_lock(_device_map_mutex);
    PointerVector<Device> devices;

    for (auto& map_entry : _device_map) {
      devices.push_back(map_entry.second);
    }

    return devices;
  }

  Pointer<Device> DeviceManagerPrivate::getDevice(uint32_t id)
  {
    USBGUARD_LOG(Trace) << "id=" << id;
    std::unique_lock<std::mutex> device_map_lock(_device_map_mutex);
    try {
      return _device_map.at(id);
    }
    catch(...) {
      throw Exception("Device lookup", "device id", "id doesn't exist");
    }
  }

  void DeviceManagerPrivate::DeviceEvent(DeviceManager::EventType event, Pointer<Device> device)
  {
    USBGUARD_LOG(Trace) << "event=" << DeviceManager::eventTypeToString(event)
                        << "device_ptr=" << device.get();
    _hooks.dmHookDeviceEvent(event, device);
  }
} /* namespace usbguard */
