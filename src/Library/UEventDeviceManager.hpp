//
// Copyright (C) 2016 Red Hat, Inc.
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
#pragma once
#include <build-config.h>

#if defined(HAVE_UEVENT)

#include "Typedefs.hpp"
#include "Common/Thread.hpp"

#include "DeviceManager.hpp"
#include "Device.hpp"
#include "Rule.hpp"
#include "LinuxSysIO.hpp"

#include <istream>

namespace usbguard {
  class UEventDeviceManager;

  class UEventDevice : public Device
  {
  public:
    UEventDevice(UEventDeviceManager& device_manager, const std::string& sys_path);
    const String& getSysPath() const;
    bool isController() const;

  protected:
    void readDescriptors(std::istream& stream);
    void readConfiguration(int c_num, std::istream& stream);
    void readInterfaceDescriptor(int c_num, int i_num, std::istream& stream);
    void readEndpointDescriptor(int c_num, int i_num, int e_num, std::istream& stream);

  private:
    String _syspath;
  };

  class UEventDeviceManager : public DeviceManager
  {
  public:
    UEventDeviceManager(DeviceManagerHooks& hooks);
    ~UEventDeviceManager();

    void setDefaultBlockedState(bool state);

    void start();
    void stop();
    void scan();

    Pointer<Device> applyDevicePolicy(uint32_t id, Rule::Target target);
    void insertDevice(Pointer<Device> device);
    Pointer<Device> removeDevice(const String& syspath);

    uint32_t getIDFromSysPath(const String& syspath) const;

  protected:
    int ueventOpen();
    void sysioApplyTarget(const String& sys_path, Rule::Target target);

    void thread();
    void ueventProcessRead();
    void ueventEnumerateDevices();

    void processDevicePresence(Pointer<UEventDevice> device);
    void processDeviceInsertion(Pointer<UEventDevice> device);
    void processDeviceRemoval(Pointer<UEventDevice> device);

  private:
    Thread<UEventDeviceManager> _thread;
    int _uevent_fd;
    int _wakeup_fd;
    StringKeyMap<uint32_t> _syspath_map;
  };
} /* namespace usbguard */
#endif /* HAVE_UEVENT */
