# Version 1.1.0

## New Features

- Add `Auth::create_secure_file` and `Auth::create_plain_file`
- Add `set_orphan()` to `Appfs::FileAttributes`
- Add `Sos::wait_pid()` method to access unistd `waitpid()`

## Bug Fixes

- Fix an `EINVAL` error when using `Appfs::append`
- Add `const` to `SerialNumber::operator==()`
- Minor cleanup for `Auth` class

# Version 1.0.0

Initial stable release.
