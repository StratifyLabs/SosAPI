# Version 1.4.5

## Bug Fixes

- Delete unused files `Cli.cpp` and `Cli.hpp`


# Version 1.4.4

## Bug Fixes

- Add version info to CMakeLists.txt

# Version 1.4.3

## Bug Fixes

- Fix build errors when `CryptoAPI` is not available
  - Requires setting `SOS_API_USE_CRYPTO_API` to `OFF`

# Version 1.4.2

## Bug Fixes

- Fix several build errors with `link` architecture

# Version 1.4.1

## Bug Fixes

- Update to `cmsdk2` and fixes required for API v1.6

# Version 1.4.0

## New Features

- port cmake files to `CMakeSDKv2.0`

# Version 1.3.0

## New Features

- Add class `sos::AppfsLog` for saving log entries to flash memory
- Update `sos::Appfs` file data to resume appending
- Add `sos::Auth::signature_marker_size` for the file size change when appending signatures

## Bug Fixes

- Fixed `Link::install_os()` where last page was written erroneously on some devices
- Fixed how `Link::DriverPath` generates paths (they must start with `@`)

# Version 1.2.0

## New Features

- Update `CMakeLists.txt` to require setup of SDK in a super-project
- Add `Auth::create_secure_file` and `Auth::create_plain_file`
- Add `set_orphan()` to `Appfs::FileAttributes`

## Bug Fixes

- Fix an `EINVAL` error when using `Appfs::append`
- Add `const` to `SerialNumber::operator==()`
- Fix include `sos/dev/auth.h` for building with Stratify OS

# Version 1.1.0

## New Features

- Add `Sos::wait_pid()` method to access unistd `waitpid()`

## Bug Fixes

- Minor cleanup for `Auth` class

# Version 1.0.0

Initial stable release.
