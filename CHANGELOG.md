# Changelog
All notable changes to this project will be documented in this file.

## 1.11 2024-08-11
### Notes
- Working with older models normally.

### Fixed
- Strcasecmp crash.
- Front panel WiFi LED not working as expected.

## 1.1 2024-08-11
### Notes
- Tested on model year 2024 (42TVAB).

### Added
- Horizontal vane swing.
- Vertical and horizontal vane swing at the same time.
- 5 Fixed vertical vane positions.
- Ability to turn on/off wifi LED on front panel.
- Fireplace mode.
- 8 Degree mode.

### Known issue
- If use with older model the MCU will randomly reboot when pulling some data from AC.

## 1.0.1 - 2022-06-06
### Added
- Ability to send a custom packet.

### Changed
- Method to read and create packet with flexible data length.
- Used "for loop" for compact some functions.
- Deprecated DATA_TYPE

### Fixed
- Wifi symbol on panel blinking because no respond after feedbacks received from hvac.

## 1.0.0 - 2022-05-27
- First released.