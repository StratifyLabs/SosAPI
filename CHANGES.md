# Version 1.2.0

## New Features

- Add class `sos::AppfsLog` for saving log entries to flash memory
- Update `sos::Appfs` file data to resume appending
- Add `sos::Auth::signature_marker_size` for the file size change when appending signatures
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
