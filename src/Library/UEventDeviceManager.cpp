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
#include <build-config.h>

#if defined(HAVE_UEVENT)
#include "UEventDeviceManager.hpp"
#include "UEventParser.hpp"
#include "LinuxSysIO.hpp"
#include "Logger.hpp"
#include "Exception.hpp"
#include "USB.hpp"

#include <stdexcept>
#include <fstream>

#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>


namespace usbguard {


  UEventDevice::UEventDevice(UEventDeviceManager& device_manager, struct udev_device* dev)
    : Device(device_manager)
  {
    /*
     * Look for the parent USB device and set the parent id
     * if we find one.
     */
    struct udev_device *parent_dev = udev_device_get_parent(dev);

    if (parent_dev == nullptr) {
      throw std::runtime_error("Cannot identify the parent device");
    }

    const char *parent_devtype = udev_device_get_devtype(parent_dev);
    const char *parent_syspath_cstr = udev_device_get_syspath(parent_dev);

    if (parent_syspath_cstr == nullptr) {
      throw std::runtime_error("Cannot retrieve syspath of the parent device");
    }

    const String parent_syspath(parent_syspath_cstr);

    if (parent_devtype == nullptr ||
        strcmp(parent_devtype, "usb_device") != 0) {
      /* The parent device is not a USB device */
      setParentID(Rule::RootID);
      setParentHash(hashString(parent_syspath));
    }
    else {
      setParentID(device_manager.getIDFromSysPath(parent_syspath));
    }

    const char *name = udev_device_get_sysattr_value(dev, "product");
    if (name) {
      setName(name);
    }

    const char *id_vendor_cstr = udev_device_get_sysattr_value(dev, "idVendor");
    const char *id_product_cstr = udev_device_get_sysattr_value(dev, "idProduct");

    if (id_vendor_cstr && id_product_cstr) {
      const String id_vendor = id_vendor_cstr;
      const String id_product = id_product_cstr;
      USBDeviceID device_id(id_vendor, id_product);
      setDeviceID(device_id);
    }

    const char *serial = udev_device_get_sysattr_value(dev, "serial");
    if (serial) {
      setSerial(serial);
    }

    /* FIXME: We should somehow lock the syspath before accessing the
     *        files inside to prevent creating invalid devices. It is
     *        possible that the device we are working with now will not
     *        be the same when we start reading the descriptor data and
     *        the authorization state.
     */
    const char *syspath = udev_device_get_syspath(dev);
    if (syspath) {
      _syspath = syspath;
    } else {
      throw std::runtime_error("device wihtout syspath");
    }

    const char *sysname = udev_device_get_sysname(dev);
    if (sysname) {

      setPort(sysname);
    } else {
      throw std::runtime_error("device wihtout sysname");
    }

    setTarget(Rule::Target::Unknown);
    std::ifstream authstate_stream(_syspath + "/authorized", std::ifstream::binary);

    if (!authstate_stream.good()) {
      throw std::runtime_error("cannot read authorization state");
    }
    else {
      switch(authstate_stream.get()) {
        case '1':
          setTarget(Rule::Target::Allow);
          break;
        case '0':
          setTarget(Rule::Target::Block);
          break;
        default:
          /* Block the device if we get an unexpected value */
          setTarget(Rule::Target::Block);
      }
    }

    std::ifstream descriptor_stream(_syspath + "/descriptors", std::ifstream::binary);

    /* Find out the descriptor data stream size */
    size_t descriptor_expected_size = 0;

    if (!descriptor_stream.good()) {
      throw std::runtime_error("Cannot load USB descriptors: failed to open the descriptor data stream");
    }
    else {
      using namespace std::placeholders;
      USBDescriptorParser parser;

      auto load_device_descriptor = std::bind(&UEventDevice::loadDeviceDescriptor, this, _1, _2);
      auto load_configuration_descriptor = std::bind(&UEventDevice::loadConfigurationDescriptor, this, _1, _2);
      auto load_interface_descriptor = std::bind(&UEventDevice::loadInterfaceDescriptor, this, _1, _2);
      auto load_endpoint_descriptor = std::bind(&UEventDevice::loadEndpointDescriptor, this, _1, _2);

      parser.setHandler(USB_DESCRIPTOR_TYPE_DEVICE, sizeof (USBDeviceDescriptor),
                        USBParseDeviceDescriptor, load_device_descriptor);
      parser.setHandler(USB_DESCRIPTOR_TYPE_CONFIGURATION, sizeof (USBConfigurationDescriptor),
                        USBParseConfigurationDescriptor, load_configuration_descriptor);
      parser.setHandler(USB_DESCRIPTOR_TYPE_INTERFACE, sizeof (USBInterfaceDescriptor),
                        USBParseInterfaceDescriptor, load_interface_descriptor);
      parser.setHandler(USB_DESCRIPTOR_TYPE_ENDPOINT, sizeof (USBEndpointDescriptor),
                        USBParseEndpointDescriptor, load_endpoint_descriptor);
      parser.setHandler(USB_DESCRIPTOR_TYPE_ENDPOINT, sizeof (USBAudioEndpointDescriptor),
                        USBParseAudioEndpointDescriptor, load_endpoint_descriptor);

      if ((descriptor_expected_size = parser.parse(descriptor_stream)) < sizeof(USBDeviceDescriptor)) {
        throw std::runtime_error("Descriptor data parsing failed: parser processed less data than the size of a USB device descriptor");
      }
    }
    /*
     * Reset descriptor stream before before computing
     * the device hash.
     *
     * Because the eofbit is set, clear() has to be called
     * before seekg().
     */
    descriptor_stream.clear();
    descriptor_stream.seekg(0);

    /*
     * Compute and set the device hash.
     */
    updateHash(descriptor_stream, descriptor_expected_size);
  }

  const String& UEventDevice::getSysPath() const
  {
    return _syspath;
  }

  bool UEventDevice::isController() const
  {
    if (getPort().substr(0, 3) != "usb" || getInterfaceTypes().size() != 1) {
      return false;
    }

    const USBInterfaceType hub_interface("09:00:*");

    return hub_interface.appliesTo(getInterfaceTypes()[0]);
  }

  /*
   * Manager
   */
  UEventDeviceManager::LinuxDeviceManager(DeviceManagerHooks& hooks)
    : DeviceManager(hooks),
      _thread(this, &UEventDeviceManager::thread)
  {
    setDefaultBlockedState(/*state=*/true);

    if ((_event_fd = eventfd(0, 0)) < 0) {
      throw std::runtime_error("eventfd init error");
    }

    if ((_uevent_fd = ueventOpen()) < 0) {
      throw std::runtime_error("ueventfd init error");
    }

    if ((_udev = udev_new()) == nullptr) {
      throw std::runtime_error("udev init error");
    }
  }

  UEventDeviceManager::~LinuxDeviceManager()
  {
    if (getRestoreControllerDeviceState()) {
      setDefaultBlockedState(/*state=*/false); // FIXME: Set to previous state
    }
    stop();
    (void)close(_uevent_fd);
    (void)close(_event_fd);
  }

  void UEventDeviceManager::setDefaultBlockedState(bool state)
  {
    sysioSetAuthorizedDefault(!state);
  }

  void UEventDeviceManager::start()
  {
    _thread.start();
  }

  void UEventDeviceManager::stop()
  {
    // stop monitor
    _thread.stop(/*do_wait=*/false);
    { /* Wakeup the device manager thread */
      const uint64_t one = 1;
      USBGUARD_SYSCALL_THROW("Linux device manager",
        write(_event_fd, &one, sizeof one) != sizeof one);
    }
    _thread.wait();
  }

  void UEventDeviceManager::scan()
  {
    if (!_thread.running()) {
      ueventEnumerateDevices();
    } else {
      throw std::runtime_error("DeviceManager thread is running, cannot perform a scan");
    }
  }

  Pointer<Device> UEventDeviceManager::applyDevicePolicy(uint32_t id, Rule::Target target)
  {
    Pointer<UEventDevice> device = std::static_pointer_cast<LinuxDevice>(getDevice(id));
    std::unique_lock<std::mutex> device_lock(device->refDeviceMutex());

    sysioApplyTarget(device->getSysPath(), target);
    device->setTarget(target);

    return std::move(device);
  }

  int UEventDeviceManager::ueventOpen()
  {
    int socket_fd = -1;

    USBGUARD_SYSCALL_THROW("UEvent device manager",
        socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT) < 0);

    try {
      struct sockaddr_nl sa = { 0 };

      sa.nl_family = AF_NETLINK;
      sa.nl_pid = getpid();
      sa.nl_groups = -1;

      USBGUARD_SYSCALL_THROW("UEvent device manager",
          bind(socket_fd, (void *)&sa, sizeof sa) != 0);
    } catch(...) {
      (void)close(socket_fd);
      throw;
    }

    return socket_fd;
  }

  void UEventDeviceManager::sysioApplyTarget(const String& sys_path, Rule::Target target)
  {
    const char *target_file = nullptr;
    int target_value = 0;

    switch (target)
      {
      case Rule::Target::Allow:
	target_file = "authorized";
	target_value = 1;
	break;
      case Rule::Target::Block:
	target_file = "authorized";
	target_value = 0;
	break;
      case Rule::Target::Reject:
	target_file = "remove";
	target_value = 1;
	break;
      default:
	//log->critical("BUG: unknown rule target");
	throw std::runtime_error("Unknown rule target in applyDevicePolicy");
      }

    char sysio_path[SYSIO_PATH_MAX];
    const int sysio_path_length = sys_path.size() + strlen(target_file) + 1;

    if (snprintf(sysio_path, SYSIO_PATH_MAX,
          "%s/%s", sys_path.c_str(), target_file) != sysio_path_length) {
      throw Exception("Linux device manager", "sysio_path", "Failed to construct path for sysioWrite");
    }
    sysioWrite(sysio_path, target_value);
  }

  void UEventDeviceManager::thread()
  {
    USBGUARD_LOG(Trace) << "Entering main loop.";

    const int max_fd = std::max(_uevent_fd, _wakeup_fd);
    fd_set readset;

    ueventEnumerateDevices(); /* scan() without thread state check */

    while (!_thread.stopRequested()) {
      struct timeval tv_timeout = { 5, 0 };

      FD_ZERO(&readset);
      FD_SET(_uevent_fd, &readset);
      FD_SET(_wakeup_fd, &readset);

      switch (select(max_fd + 1, &readset, NULL, NULL, &tv_timeout))
      {
        case 1: /* Device or event */
        case 2: /* Device and event */
	  if (FD_ISSET(_wakeup_fd, &readset)) {
            USBGUARD_LOG(Debug) << "Wakeup event.";
	    continue;
	  }
	  if (FD_ISSET(_uevent_fd, &readset)) {
            USBGUARD_LOG(Debug) << "UEvent read event."; 
	    ueventProcessRead();
	  }
	  break;
        case 0: /* Timeout */
	  continue;
        case -1: /* Error */
        default:
          USBGUARD_LOG(Error) << "UEventDeviceManager thread: select failed: errno=" << errno;
	  _thread.stop();
      }
    } /* Thread main loop */
    USBGUARD_LOG(Trace) << "Leaving main loop.";
  }

  void UEventDeviceManager::ueventProcessRead()
  {
    std::string buffer(4096, 0);

    const int recv_size = recv(_uevent_fd, &buffer[0], buffer.capacity(), MSG_DONTWAIT);

    if (recv_size <= 0) {
      const int saved_errno = errno;
      if (saved_errno == EAGAIN || saved_errno == EWOULDBLOCK) {
        USBGUARD_LOG(Warning) << "ueventProcessRead: "
                              << "reading from uevent source would block thread execution";
        return;
      }
      else {
        USBGUARD_LOG(Error) << "ueventProcessRead: "
                            << "failed to read pending uevent: "
                            << "recv_size=" << recv_size << " errno=" << saved_errno;
        throw SyscallException();
      }
    }

    buffer.resize(recv_size);

    // Parse the uevent
    // Filter
    // Create UEventDevice
    // Process the device

    struct udev_device *dev = udev_monitor_receive_device(_umon);

    if (!dev) {
      return;
    }

    const char *action_cstr = udev_device_get_action(dev);

    if (!action_cstr) {
      udev_device_unref(dev);
      return;
    }

    if (strcmp(action_cstr, "add") == 0) {
      processDeviceInsertion(dev);
    }
    else if (strcmp(action_cstr, "remove") == 0) {
      processDeviceRemoval(dev);
    }

    udev_device_unref(dev);
  }

  void UEventDeviceManager::udevEnumerateDevices()
  {
    struct udev_enumerate *enumerate = udev_enumerate_new(_udev);

    if (enumerate == nullptr) {
      throw std::runtime_error("udev_enumerate_new returned NULL");
    }

    udev_enumerate_add_match_subsystem(enumerate, "usb");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry *dlentry = nullptr;

    udev_list_entry_foreach(dlentry, devices) {
      const char *syspath = udev_list_entry_get_name(dlentry);

      if (syspath == nullptr) {
	continue;
      }

      struct udev_device *device = udev_device_new_from_syspath(_udev, syspath);

      if (device == nullptr) {
        USBGUARD_LOG(Warning) << "Cannot create a new device from syspath=" << syspath
                              << ". Ignoring.";
	continue;
      }

      const char *devtype = udev_device_get_devtype(device);

      if (devtype == nullptr) {
        USBGUARD_LOG(Warning) << "Cannot get device type for device at syspath=" << syspath
                              << ". Ignoring.";
	udev_device_unref(device);
	continue;
      }

      if (strcmp(devtype, "usb_device") == 0) {
	processDevicePresence(device);
      }

      udev_device_unref(device);
    }

    udev_enumerate_unref(enumerate);
  }

  void UEventDeviceManager::processDevicePresence(struct udev_device *dev)
  {
    const String sys_path(udev_device_get_syspath(dev));
    try {
      Pointer<UEventDevice> device = makePointer<LinuxDevice>(*this, dev);
      insertDevice(device);
      DeviceEvent(DeviceManager::EventType::Present, device);
      return;
    }
    catch(const Exception& ex) {
      USBGUARD_LOG(Error) << "Present device exception: " << ex.message();
    }
    catch(const std::exception& ex) {
      USBGUARD_LOG(Error) << "Present device exception: " << ex.what();
    }
    catch(...) {
      USBGUARD_LOG(Error) << "BUG: Unknown device exception.";
    }
    /*
     * We don't reject the device here (as is done in processDeviceInsertion)
     * because the device was already connected to the system when USBGuard
     * started. Therefore, if the device is malicious, it already had a chance
     * to interact with the system.
     */
  }

  void UEventDeviceManager::processDeviceInsertion(struct udev_device *dev)
  {
    const String sys_path(udev_device_get_syspath(dev));
    try {
      Pointer<UEventDevice> device = makePointer<LinuxDevice>(*this, dev);
      insertDevice(device);
      DeviceEvent(DeviceManager::EventType::Insert, device);
      return;
    }
    catch(const Exception& ex) {
      USBGUARD_LOG(Error) << "Device insert exception: " << ex.message();
    }
    catch(const std::exception& ex) {
      USBGUARD_LOG(Error) << "Device insert exception: " << ex.what();
    }
    catch(...) {
      USBGUARD_LOG(Error) << "BUG: Unknown device insert exception.";
    }
    /*
     * Something went wrong and an exception was generated.
     * Either the device is malicious or the system lacks some
     * resources to successfully process the device. In either
     * case, we take the safe route and fallback to rejecting
     * the device.
     */
    USBGUARD_LOG(Warning) << "Rejecting device at syspath=" << sys_path;
    sysioApplyTarget(sys_path, Rule::Target::Reject);
  }

  void UEventDeviceManager::insertDevice(Pointer<Device> device)
  {
    DeviceManager::insertDevice(device);
    std::unique_lock<std::mutex> device_lock(device->refDeviceMutex());
    _syspath_map[std::static_pointer_cast<UEventDevice>(device)->getSysPath()] = device->getID();
  }

  void UEventDeviceManager::processDeviceRemoval(struct udev_device *dev)
  {
    USBGUARD_LOG(Trace) << "dev=" << dev;

    const char *syspath_cstr = udev_device_get_syspath(dev);
    if (!syspath_cstr) {
      USBGUARD_LOG(Debug) << "Syspath attribute not available.";
      return;
    }
    const String syspath(syspath_cstr);
    try {
      Pointer<Device> device = removeDevice(syspath);
      DeviceEvent(DeviceManager::EventType::Remove, device);
    } catch(...) {
      /* Ignore for now */
      USBGUARD_LOG(Debug) << "Removal of an unknown device ignored.";
      return;
    }
  }

  Pointer<Device> UEventDeviceManager::removeDevice(const String& syspath)
  {
    /* FIXME: device map locking */
    auto it = _syspath_map.find(syspath);
    if (it == _syspath_map.end()) {
      throw std::runtime_error("Unknown device, cannot remove from syspath map");
    }
    const uint32_t id = it->second;
    Pointer<Device> device = DeviceManager::removeDevice(id);
    _syspath_map.erase(it);
    return device;
  }

  uint32_t UEventDeviceManager::getIDFromSysPath(const String& syspath) const
  {
    return _syspath_map.at(syspath);
  }
} /* namespace usbguard */
#endif /* HAVE_UDEV */
