# Change Log

## 0.6.3 - [unreleased]
### Changed

### Added
- Added InsertedDevicePolicy configuration option to control the policy
  method for inserted devices
- Added RestoreControllerDeviceState configuration option
- Added DeviceManagerBackend configuration option. This option can be used
  to select from several device manager backend implementations.

### Removed

## 0.6.2 - 2016-09-18
### Changed
- Wait for disconnect in IPCClient dtor if needed
- Qt Applet: Fixed loading of decision method and default decision settings

## 0.6.1 - 2016-09-16
### Changed
- Refactored logging subsystem
- Fixed handling of IPC disconnect in the IPCClient class
- Qt Applet: Fixed handling of main window minimization and maximization
- Fixed building on architectures that don't provide required atomic operations.
  The libatomic emulation library will be used in such cases.
- Fixed several typos in the documentation

### Added
- Implemented a simple internal logger
- Access to the logger via public API
- Improved logging coverage. Logging output can be enabled either via
  CLI options or by setting the `USBGUARD_DEBUG` environment variable to 1.
- Qt Applet: UI translation support.
- Qt Applet: Czech (cs_CZ) translation

### Removed
- Removed spdlog dependency

## 0.6.0 - 2016-09-04
### Changed
- Fixed the daemon to use the match target when matching device rules.
  This fixes matching devices against the rules in the policy.
- Switched to protobuf based IPC
- Simplified the IPC and D-Bus interfaces
- Refactored custom exceptions

### Added
- Added DevicePresenceChanged signal for notifying about device
  insertions, updates and removals
- Added DevicePolicyChanged signal for notifying about device
  policy changes.
- Added ExceptionMessage signal for sending asynchronous exceptions
  to the IPC/D-Bus clients.
- Extended the usbguard CLI watch subcommand with the ability to
  wait for the IPC connection to become available. See the new `-w`
  and `-o` options for details.

### Removed
- Removed nlohmann/json submodule
- Removed allowDevice, blockDevice and rejectDevice methods from
  the IPC and D-Bus interface.
- Removed DeviceAllowed, DeviceBlocked and DeviceRejected signals
  from the IPC and D-Bus interface (replaced by single signal)
- Removed DeviceInserted, DevicePresent and DeviceRemoved signals
  from the IPC and D-Bus interface (replaced by single signal)

## 0.5.14 - 2016-08-18
### Changed
- Fixed unknown descriptor type handling

## 0.5.13 - 2016-08-16
### Changed
- refactored USB parser to support multiple handlers per USB descriptor type
- Qt Applet: device list should be always expanded

### Added
- added support for an audio device related endpoint descriptor

## 0.5.12 - 2016-08-14
### Changed
- Fixed a bug in matching USB interface types with wildcards
- usbguard-daemon will now abort at startup if the rule file contains
  syntax error instead of continuing with an empty rule set

## 0.5.11 - 2016-08-13
### Added
- Maintainer script for spellchecking source archive files
- Rule::Attribute class for representing the rule attributes
- USBDeviceID class for representing the USB device ID
- configure script option to control the bundling of PEGTL source files
- id attribute to the rule language for specifying the USB device ID
- Added a parent device ID field (and methods) to the Device class which
  tracks the ID of the parent device
- Implemented "parent-hash" attribute for associating a device with its
  parent device.
- The QtSvg module/library is now needed to compile the Qt applet
- Qt Applet: Device Dialog settings work now
- Qt Applet: The settings state is now remembered (via Qt's QSettings class)
- Qt Applet: Implemented serial number masking
- Qt Applet: Initial implementation of DeviceDialog window position randomization
- Qt Applet: IPC exceptions are shown in the message log
- Qt Applet: added an initial implementation of the device list with the ability
             to change authorization target for each device
- Qt Applet: show a grey version of the USBGuard icon in IPC disconnected state
- usbguard-daemon.conf: added DeviceRulesWithPort setting (set to false by default)
- Added support for selecting crypto backend library at compile time using the
  --with-crypto-library configure script switch

### Changed
- **IMPORTANT**: The device hash value computation was changed to include the
  USB descriptor data. Additionally, the algorithm was changed to SHA-256 and
  the hash value representation to base64. These changes are backwards incompatible
  and existing policies that use the hash attribute need to be updated.
- Reimplemented the rule parser using PEGTL
- Changed public API of the Rule and Device classes because of the new
  Rule::Attribute class rule attribute representation
- Extended the public IPCClient::IPCDisconnected method to include exception
  related information
- All rule attributes now support both the single and multivalued form
- A rule attribute can now be specified only once
- The default usbguard-daemon.conf and usbguard.service files now respect
  the paths set by the configure script
- New Qt applet icons with
- Fixed Qt applet so that it doesn't show an empty window when starting
- Qt Applet: Reject button is hidden by default.
- Updated usbguard-daemon manual page
- Permanent device specific rules managed by allowDevice, blockDevice and
  rejectDevice actions are now handled properly. Existing device rules are
  updated instead of just appending new rules to the policy.
- usbguard-daemon.conf: changed the default configuration value of
  PresentControllerPolicy to keep
- Changed the device hashing algorithm to SHA-256
- Switched hash value representation from hex to base64

### Removed
- Removed Quex related files
- The "from Rule" Device class constructor was removed because it's use case
  is unclear and it wasn't used anywhere

## 0.5.10 - 2016-07-08
### Changed
- Changed versioning scheme to include a release number. Releases will be
  rolled out more often during a milestone (x.y -- currently 0.5) development.
- Enhancements of the usbguard command-line interface.
- Manual pages are now generated from their source (markdown) during the build
  process.
- A default usbguard-daemon configuration file is now installed.

### Added
- Merged usbguard-applet-qt (sub)project into usbguard.
- Added a D-Bus bridge component which exposes a D-Bus interface to usbguard.

### Fixed
- Fixed USB descriptor data parsing. Previously, many real-world cases of
  descriptors weren't parsed correctly because of a naive implementation of the
  parser.
- Fixed systemd integration.
- Fixed issues: #84, #83, #80, #79, #78, #77, #76, #75, #74, #73, #72, #71,
  #70, #69, #68, #56, #54, #53, #51, #50, #44.

## 0.4 - 2016-02-07
### Changed
- The daemon is now capable of dropping process capabilities and uses a seccomp
  based syscall whitelist. Options to enable these features were added to the
  usbguard-daemon command.
- Devices connected at the start of the daemon are now recognized and the
  DevicePresent signal is sent for each of them.
- The library API was extended with the Device and DeviceManager classes.
- Implemented the usbguard CLI, see usbguard(1) for available commands.
- Initial authorization policies can be now easily generated using the usbguard
  generate-policy command.
- Extended the rule language with rule conditions. See usbguard-rules.conf(5)
  for details.
- Moved logging code into the shared library. You can use static methods of the
  Logger class to configure logging behaviour.
- Removed the bundled libsodium and libqb libraries.

### Added
- New configuration options for setting the implicit policy target and how to
  handle the present devices are now available.

### Fixed
- String values read from the device are now properly escaped and length limits
  on these values are enforced.
- Fixed several bugs.
- Resolved issues: #46, #45, #41, #40, #37, #32, #31, #28, #25, #24, #21, #16,
  #13, #9, #4

### Removed
- The device hashing procedure was altered and generates different hash values.
  If you are using the hash attribute in your rules, you'll have to update the
  values.
- The bundled libsodium and libqb were removed. You'll have to compile and
  install them separately if your distribution doesn't provide them as
  packages.

## 0.3p3 - 2015-04-14
### Changed
- use AC_CHECK_HEADER instead of a pkg-config based check for json and spdlog
- make check target available

## 0.3p2 - 2015-04-02
### Changed
- SHM permissions are set to uid of the client process and gid of the daemon
  with rw-rw---- mode (0660).
- added --with-bundled-json and --with-bundled-spdlog configure options to
  enable/disable the usage of the bundled libraries

## 0.3p1 - 2015-04-10
### Removed
- removed bundled cppformat copylib

## 0.3 - 2015-04-09
### Changed
- Implemented a DAC based IPC access control

## Added
- Ship man pages for usbguard-daemon, usbguard-daemon.conf and
  usbguard-rules.conf
- Ship the LICENSE file

### Fixed
- Fixed appending of permanent rules
- Fixed distribution RPM spec file. Thanks to Petr Lautrbach and Ralf
  Corsepius for review.
- Resolved issues: #18 #19 #13

## 0.2 - 2015-04-03
### Changed
- Reworked device hashing.
- Rule language changes
  - set operators
  - renamed "port" to "via-port"
  - added "with-interface" matching attribute
  - removed the "class" attribute
- The IPCClient, ConfigFile, Rule and RuleSet classes are now shipped in a
  shared library.
- Created abstract interface for OS specific USB device handling.
- Changed default daemon config path to /etc/usbguard/usbguard-daemon.conf.
- Implemented basic USB descriptor structure parsing and improved
  interface type handling
- The IPC API was changed:
  - added new signal, DevicePresent, which signals that a device was
    already present at the start of the IPC session
  - the DeviceInserted and DevicePresent signals pass interface types that
    the device supports
  - the explicit string arguments of the signals are now passed as a map

### Added
- Support for modifying permanent rules over the IPC interface.

### Fixed
- Resolved issues: #1 #2 #5 #6 #10 #11

