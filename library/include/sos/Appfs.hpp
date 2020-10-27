/*! \file */ // Copyright 2011-2020 Tyler Gilbert and Stratify Labs, Inc; see
             // LICENSE.md for rights.

#ifndef SYSAPI_SYS_APPFS_HPP_
#define SYSAPI_SYS_APPFS_HPP_

#include "api/api.hpp"

#include "fs/File.hpp"
#include "var/String.hpp"

namespace sos {

class AppfsFlags {
public:
  enum Flags {
    is_default = 0,
    is_flash = APPFS_FLAG_IS_FLASH,
    is_startup = APPFS_FLAG_IS_STARTUP,
    is_authenticated = APPFS_FLAG_IS_AUTHENTICATED,
    is_replace = APPFS_FLAG_IS_REPLACE,
    is_orphan = APPFS_FLAG_IS_ORPHAN,
    is_unique = APPFS_FLAG_IS_UNIQUE,
    is_code_external = APPFS_FLAG_IS_CODE_EXTERNAL,
    is_data_external = APPFS_FLAG_IS_DATA_EXTERNAL,
    is_code_tightly_coupled = APPFS_FLAG_IS_CODE_TIGHTLY_COUPLED,
    is_data_tightly_coupled = APPFS_FLAG_IS_DATA_TIGHTLY_COUPLED
  };
};

API_OR_NAMED_FLAGS_OPERATOR(AppfsFlags, Flags)

/*! \brief Application File System Class
 * \details This class provides an interface for creating data files in flash
 * memory.
 *
 * The following is a basic example of creating a data file in flash and then
 * reading from it.
 *
 * \code
 * #include <sapi/sys.hpp>
 *
 * int main(int argc, char * argv[]){
 * 	char my_user_data[16];
 * 	//populate my_user_data as you please here
 * 	sprintf(my_user_data, "Hello World!");
 * 	Appfs::create("my_data", my_user_data, 16); //creates /app/flash/my_data
 * as read only data
 * }
 * \endcode
 *
 * Now the file can be read just like any other file would be.  Of
 * course, reading files from /app/flash is very fast compared
 * to filesystems built on external chips.
 *
 * \code
 * #include <sapi/sys.hpp>
 *
 * int main(int argc, char * argv[]){
 * 	char my_user_data[16];
 * 	File f;
 * 	f.open("/app/flash/my_data", File::RDONLY); //only the file read only
 * 	f.read(my_user_data, 16); //read 16 bytes of user data
 * 	f.close();  //free resources
 * 	return 0;
 * }
 * \endcode
 *
 */
class Appfs : public api::ExecutionContext, public AppfsFlags {
public:
  /*! \brief AppfsInfo Class
   * \details The AppfsInfo class is for
   * getting information associated with
   * executable files that are either installed
   * in or built for the application filesystem.
   *
   */
  class Info : public AppfsFlags {
  public:
    /*! \details Constructs an empty object. */
    Info() { memset(&m_info, 0, sizeof(m_info)); }

    /*! \details Constructs an object from a *appfs_info_t* object. */
    explicit Info(const appfs_info_t &info) {
      memcpy(&m_info, &info, sizeof(appfs_info_t));
    }

    /*! \details Returns true if the object is valid. */
    bool is_valid() const { return m_info.signature != 0; }

    /*! \details Returns the application ID (cloud id). */
    const var::StringView id() const {
      return var::StringView(reinterpret_cast<const char *>(m_info.id));
    }

    /*! \details Returns the name of the application. */
    const var::StringView name() const {
      return var::StringView(reinterpret_cast<const char *>(m_info.name));
    }

    /*! \details Returns the file mode. */
    u16 mode() const { return m_info.mode; }
    /*! \details Returns the version. */
    u16 version() const { return m_info.version; }

    /*! \details Returns the data RAM size used by the application. */
    u32 ram_size() const { return m_info.ram_size; }

    /*! \details Returns the flags.
     *
     * See also: is_executable(), is_startup(), is_flash(), is_orphan(),
     * is_root(), is_unique().
     *
     */
    u32 o_flags() const { return m_info.o_flags; }

    /*! \details Returns the application signature.
     *
     * This value specifies the version of the table that
     * connects the application to the operating system. Every
     * OS package has a signature that is associated with
     * the calls (such as printf(), pthread_create()) that are available to
     * application installed on the system.
     *
     */
    u32 signature() const { return m_info.signature; }

    /*! \details Returns true if the application is executable. */
    bool is_executable() const { return m_info.mode & 0111; }

    /*! \details Returns true if the application runs at startup. */
    bool is_startup() const {
      return (m_info.o_flags & Flags::is_startup) != 0;
    }
    /*! \details Returns true if the application is to be installed in flash. */
    bool is_flash() const { return (m_info.o_flags & Flags::is_flash) != 0; }
    /*! \details Returns true if the application code is to be installed in
     * external memory. */
    bool is_code_external() const {
      return (m_info.o_flags & Flags::is_code_external) != 0;
    }
    /*! \details Returns true if the application data is to be installed in
     * external memory. */
    bool is_data_external() const {
      return (m_info.o_flags & Flags::is_data_external) != 0;
    }
    /*! \details Returns true if the application code is to be installed in
     * tightly coupled memory. */
    bool is_code_tightly_coupled() const {
      return (m_info.o_flags & Flags::is_code_tightly_coupled) != 0;
    }
    /*! \details Returns true if the application data is to be installed in
     * tightly coupled memory. */
    bool is_data_tightly_coupled() const {
      return (m_info.o_flags & Flags::is_data_tightly_coupled) != 0;
    }
    /*! \details Returns true if the application should run as an orphan. */
    bool is_orphan() const { return (m_info.o_flags & Flags::is_orphan) != 0; }
    /*! \details Returns true if the application should run as root. */
    bool is_authenticated() const {
      return (m_info.o_flags & Flags::is_authenticated) != 0;
    }
    /*! \details Returns true if the application should create a unique
     * instance.
     *
     * If unique is false, the system will not allow a second copy of
     * the application to be installed on the system.
     *
     * If unique is true, the application will be assigned a unique
     * name when it is installed in RAM or flash.
     *
     */
    bool is_unique() const { return (m_info.o_flags & Flags::is_unique) != 0; }

    const appfs_info_t &info() const { return m_info; }
    appfs_info_t &info() { return m_info; }

  private:
    appfs_info_t m_info;
  };

  /*! \brief Appfs::FileAttributes Class
   * \details The Appfs::FileAttributes class holds the
   * information that is needed to modify an application
   * binary that has been built with the compiler.
   *
   * The compiler is unable to build some information
   * directly into the binary but it allocates space
   * for the information.
   *
   * This class is used for that information and includes
   * things like the application name, project id,
   * and execution flags.
   *
   *
   */
  class FileAttributes : public AppfsFlags {
  public:
    FileAttributes(const fs::FileObject &existing);

    explicit FileAttributes(const appfs_file_t &appfs_file);

    const FileAttributes &apply(const fs::FileObject &file) const;

    Flags flags() const {
      return static_cast<Flags>(m_file_header.exec.o_flags);
    }

    bool is_flash() const { return flags() & Flags::is_flash; }
    bool is_code_external() const { return flags() & Flags::is_code_external; }
    bool is_data_external() const { return flags() & Flags::is_data_external; }
    bool is_code_tightly_coupled() const {
      return flags() & Flags::is_code_tightly_coupled;
    }
    bool is_data_tightly_coupled() const {
      return flags() & Flags::is_data_tightly_coupled;
    }
    bool is_startup() const { return flags() & Flags::is_startup; }
    bool is_unique() const { return flags() & Flags::is_unique; }
    bool is_authenticated() const { return flags() & Flags::is_authenticated; }

    FileAttributes &set_startup(bool value = true) {
      return set_flag_value(Flags::is_startup, value);
    }

    FileAttributes &set_flash(bool value = true) {
      return set_flag_value(Flags::is_flash, value);
    }

    FileAttributes &set_code_external(bool value = true) {
      return set_flag_value(Flags::is_code_external, value);
    }

    FileAttributes &set_data_external(bool value = true) {
      return set_flag_value(Flags::is_data_external, value);
    }

    FileAttributes &set_code_tightly_coupled(bool value = true) {
      return set_flag_value(Flags::is_code_tightly_coupled, value);
    }

    FileAttributes &set_data_tightly_coupled(bool value = true) {
      return set_flag_value(Flags::is_data_tightly_coupled, value);
    }

    FileAttributes &set_unique(bool value = true) {
      return set_flag_value(Flags::is_unique, value);
    }

    FileAttributes &set_authenticated(bool value = true) {
      return set_flag_value(Flags::is_authenticated, value);
    }

    var::StringView name() const { return m_file_header.hdr.name; }
    FileAttributes &set_name(const var::StringView value) {
      var::View(m_file_header.hdr.name).copy(value);
      return *this;
    }

    var::StringView id() const { return m_file_header.hdr.id; }
    FileAttributes &set_id(const var::StringView value) {
      var::View(m_file_header.hdr.id).copy(value);
      return *this;
    }

    u32 ram_size() const { return m_file_header.exec.ram_size; }
    FileAttributes &set_ram_size(u32 value) {
      m_file_header.exec.ram_size = value;
      return *this;
    }

    u16 version() const { return m_file_header.hdr.version; }
    FileAttributes &set_version(u32 value) {
      m_file_header.hdr.version = value;
      return *this;
    }

    u16 access_mode() const { return m_file_header.hdr.mode; }
    FileAttributes &set_access_mode(u32 value) {
      m_file_header.hdr.mode = value;
      return *this;
    }

  private:
    void assign_flags(Flags flags) {
      m_file_header.exec.o_flags = static_cast<u32>(flags);
    }

    FileAttributes &set_flag_value(Flags flag, bool value) {
      Flags a = flags();
      if (value) {
        a |= flag;
      } else {
        a &= ~flag;
      }
      assign_flags(a);
      return *this;
    }

    appfs_file_t m_file_header;
  };

  class Construct {
  public:
    Construct() : m_mount("/app") {}

  private:
    API_ACCESS_COMPOUND(Construct, var::StringView, name);
    API_ACCESS_COMPOUND(Construct, var::StringView, mount);
    API_ACCESS_FUNDAMENTAL(Construct, u32, size, 0);
    API_ACCESS_BOOL(Construct, executable, false);
    API_ACCESS_BOOL(Construct, overwrite, false);
  };

  Appfs(const Construct &options FSAPI_LINK_DECLARE_DRIVER_NULLPTR_LAST);
  Appfs(FSAPI_LINK_DECLARE_DRIVER_NULLPTR);

  Appfs &append(
    const fs::FileObject &file,
    const api::ProgressCallback *progress_callback = nullptr);

  bool is_append_ready() const { return m_bytes_written < m_data_size; }

  bool is_valid() const { return m_data_size != 0; }
  u32 size() const { return m_data_size - sizeof(appfs_file_t); }

  u32 bytes_written() const { return m_bytes_written; }
  u32 bytes_available() const { return m_data_size - m_bytes_written; }

  bool is_flash_available();
  bool is_ram_available();

  /*! \details Returns the page size for writing data. */
  static constexpr int page_size() { return APPFS_PAGE_SIZE; }
  static constexpr u32 overhead() { return sizeof(appfs_file_t); }

  /*! \details Gets the info associated with an executable file.
   *
   * @param path The path to the file
   * @param info A reference to the destination info
   * @return Zero on success
   *
   * This method will work if the file is installed in the
   * application filesystem or on a normal data filesystem.
   *
   * The method will return less than zero if the file is not
   * a recognized executable file.
   *
   * The errno will be set to ENOENT or ENOEXEC if the file
   * does not exist or is not a recognized executable, respectively.
   *
   */
  Info get_info(var::StringView path);

#if !defined __link

  enum class CleanData { no, yes };

  Appfs &cleanup(CleanData clean_data);

  /*! \details Frees the RAM associated with the app without deleting the code
   * from flash (should not be called when the app is currently running).
   *
   * @param path The path to the app (use \a exec_dest from launch())
   * @param driver Used with link protocol only
   * @return Zero on success
   *
   * This method can causes problems if not used correctly. The RAM associated
   * with the app will be free and available for other applications. Any
   * applications that are using the RAM must quit before the RAM can be
   * reclaimed using reclaim_ram().
   *
   * \sa reclaim_ram()
   */
  Appfs &free_ram(var::StringView path);

  /*! \details Reclaims RAM that was freed using free_ram().
   *
   * @param path The path to the app
   * @param driver Used with link protocol only
   * @return Zero on success
   *
   * \sa free_ram()
   */
  Appfs &reclaim_ram(var::StringView path);

#endif

private:
#if defined __link
  API_AF(Appfs, link_transport_mdriver_t *, driver, nullptr);
#endif

  fs::File m_file;
  appfs_createattr_t m_create_install_attributes = {0};
  u32 m_bytes_written = 0;
  u32 m_data_size = 0;
  int m_request = I_APPFS_CREATE;

  void create_asynchronous(const Construct &options);
  void append(var::View blob);
};

} // namespace sos

namespace printer {
class Printer;
Printer &operator<<(Printer &printer, const sos::Appfs::Info &a);
Printer &operator<<(Printer &printer, const sos::Appfs::FileAttributes &a);
Printer &operator<<(Printer &printer, const appfs_file_t &a);
} // namespace printer

#endif /* SYSAPI_SYS_APPFS_HPP_ */
