
#include <cstdio>

#include <chrono.hpp>
#include <fs.hpp>
#include <printer.hpp>
#include <sys.hpp>
#include <test/Test.hpp>
#include <var.hpp>

#include <usb/usb_link_transport_driver.h>

#include "sos.hpp"

class UnitTest : public test::Test {
public:
  UnitTest(var::StringView name) : test::Test(name) {}

  bool execute_class_api_case() {
    TEST_ASSERT(link_case());
    TEST_ASSERT(appfs_case());

    return true;
  }

  bool appfs_case() {

    Link link;
    usb_link_transport_load_driver(link.driver());

    FileSystem device_fs(link.driver());
    auto list = link.get_info_list();
    TEST_ASSERT(list.count() > 0);

    TEST_ASSERT(link.connect(list.front().path()).is_success());
    if (link.is_bootloader()) {
      TEST_ASSERT(link.reset().reconnect().is_success());
    }

    printer().object("system", link.info());

    device_fs.remove("/app/flash/HelloWorld");
    API_RESET_ERROR();
    device_fs.remove("/app/ram/HelloWorld");
    API_RESET_ERROR();

    {
      const StringView hello_world_binary_path
        = "../tests/HelloWorld_build_release_v7em_f4sh";

      const u32 ram_size = 16384;
      {
        Appfs::FileAttributes(File(hello_world_binary_path))
          .set_ram_size(ram_size)
          .set_flash(false)
          .apply(File(hello_world_binary_path, OpenMode::read_write()));

        Appfs::Info local_info = Appfs().get_info(hello_world_binary_path);

        printer().object("localRamInfo", local_info);

        TEST_ASSERT(
          Appfs(
            Appfs::Construct().set_executable(true).set_name("HelloWorld"),
            link.driver())
            .append(File(hello_world_binary_path))
            .is_success());
        PRINTER_TRACE(printer(), "");

        TEST_ASSERT(device_fs.exists("/app/ram/HelloWorld"));
        Appfs::Info info = Appfs(link.driver()).get_info("/app/ram/HelloWorld");
        printer().key("ramSize", NumberString(info.ram_size()));
        TEST_ASSERT(info.ram_size() == ram_size);

        TEST_ASSERT(device_fs.remove("/app/ram/HelloWorld").is_success());
      }

      {
        Appfs::FileAttributes(File(hello_world_binary_path))
          .set_flash()
          .apply(File(hello_world_binary_path, OpenMode::read_write()));

        Appfs::Info local_info = Appfs().get_info(hello_world_binary_path);
        printer().object("localFlashInfo", local_info);

        TEST_ASSERT(
          Appfs(
            Appfs::Construct().set_executable(true).set_name("HelloWorld"),
            link.driver())
            .append(File(hello_world_binary_path))
            .is_success());
        PRINTER_TRACE(printer(), "");
        const StringView device_path = "/app/flash/HelloWorld";
        TEST_ASSERT(device_fs.exists(device_path));
        Appfs::Info info = Appfs(link.driver()).get_info(device_path);
        printer().object("deviceFlashInfo", info);

        TEST_ASSERT(info.ram_size() == ram_size);
        TEST_ASSERT(device_fs.get_info(device_path).is_file());
        printer().object("flashFileInfo", device_fs.get_info(device_path));
        TEST_ASSERT(device_fs.remove(device_path).is_success());
      }

      {

        TEST_ASSERT(Appfs(
                      Appfs::Construct()
                        .set_executable(false)
                        .set_name("HelloWorld")
                        .set_size(File(hello_world_binary_path).size()),
                      link.driver())
                      .append(File(hello_world_binary_path))
                      .is_success());

        const StringView device_path = "/app/flash/HelloWorld";

        TEST_ASSERT(
          DataFile().write(File(hello_world_binary_path)).data()
          == DataFile()
               .write(File(device_path, OpenMode::read_only(), link.driver()))
               .data());

        TEST_ASSERT(device_fs.get_info(device_path).is_file());
      }

      TEST_ASSERT(Appfs(link.driver()).is_flash_available());
      TEST_ASSERT(Appfs(link.driver()).is_ram_available());

      return true;
    }

    {
      Appfs appfs(link.driver());

      Appfs::Info info = appfs.get_info("/app/flash/HelloWorld");
      TEST_ASSERT(is_success());

      printer().object("info", info);
      TEST_ASSERT(info.name() == "HelloWorld");
      TEST_ASSERT(Version::from_u16(info.version()).string_view() == "1.30");
      TEST_ASSERT(info.signature() == 0x384);
      TEST_ASSERT(info.is_orphan() == false);
      TEST_ASSERT(info.is_flash() == true);
      TEST_ASSERT(info.is_startup() == false);
      TEST_ASSERT(info.is_unique() == false);
    }

    return true;
  }

  bool link_case() {
    TEST_ASSERT(link_connect_case());
    TEST_ASSERT(link_path_case());
    TEST_ASSERT(link_driver_path_case());
    TEST_ASSERT(link_os_case());
    return true;
  }

  bool link_os_case() {
    Link link;
    // link_set_debug(1000);
    usb_link_transport_load_driver(link.driver());

    auto list = link.get_info_list();
    TEST_ASSERT(list.count() > 0);
    TEST_ASSERT(link.connect(list.front().path()).is_success());

    if (link.is_bootloader() == false) {
      TEST_ASSERT(
        link.reset_bootloader().reconnect(10, 200_milliseconds).is_bootloader()
        == true);
    }

    const StringView binary_path = "../tests/Nucleo-F446ZE.bin";
    TEST_ASSERT(FileSystem().exists(binary_path));

    File image(binary_path);
    TEST_ASSERT(link(Link::UpdateOs().set_image(&image).set_printer(&printer()))
                  .is_success());
    TEST_ASSERT(link.reset().reconnect().is_success());
    printer().object("info", link.info());
    return true;
  }

  bool link_connect_case() {

    Link link;
    usb_link_transport_load_driver(link.driver());
    link_set_debug(0);
    TEST_ASSERT(link.connect("/usb/2000/0001").is_error());
    API_RESET_ERROR();
    TEST_ASSERT(link.reset().is_error());
    API_RESET_ERROR();
    TEST_ASSERT(link.ping("/usb/2000/0001") == false);
    TEST_ASSERT(is_success());

    auto list = link.get_info_list();
    TEST_ASSERT(list.count() > 0);

    TEST_ASSERT(link.ping(list.front().path()) == true);
    TEST_ASSERT(is_success());

    printer().array("list", list);
    for (const auto &device : list) {
      TEST_ASSERT(link.connect(device.path()).is_success());

      TEST_ASSERT(link.disconnect().is_success());
    }

    TEST_ASSERT(link.connect(list.front().path()).is_success());
    TEST_ASSERT(link.reset().is_success());

    TEST_ASSERT(link.reconnect(10, 200_milliseconds).is_success());
    printer().object("info", link.info());
    TEST_ASSERT(
      list.front().serial_number()
      == link.info().sys_info().serial_number().to_string());

    TEST_ASSERT(link.reset_bootloader().is_success());
    TEST_ASSERT(link.reconnect(10, 200_milliseconds).is_bootloader());
    TEST_ASSERT(link.is_connected());
    TEST_ASSERT(
      link.reset().reconnect(10, 200_milliseconds).is_bootloader() == false);
    TEST_ASSERT(is_success());
    TEST_ASSERT(link.is_connected());
    return true;
  }

  bool link_driver_path_case() {
    Link::DriverPath driver_path(Link::DriverPath::Construct()
                                   .set_type(Link::Type::usb)
                                   .set_vendor_id("0x4100")
                                   .set_product_id("0x0001")
                                   .set_serial_number("12345")
                                   .set_interface_number("0")
                                   .set_device_path("/dev/serial.tty"));

    printer().key("path", driver_path.path());
    TEST_ASSERT(Link::DriverPath::is_valid(driver_path.path()));
    TEST_ASSERT(driver_path.get_type() == Link::Type::usb);
    TEST_ASSERT(driver_path.get_driver_name() == "usb");
    TEST_ASSERT(driver_path.get_vendor_id() == "0x4100");
    TEST_ASSERT(driver_path.get_product_id() == "0x0001");
    TEST_ASSERT(driver_path.get_serial_number() == "12345");
    TEST_ASSERT(driver_path.get_interface_number() == "0");
    TEST_ASSERT(driver_path.get_device_path() == "/dev/serial.tty");

    {
      Link::DriverPath string_driver_path(driver_path.path());
      TEST_ASSERT(Link::DriverPath::is_valid(string_driver_path.path()));
      TEST_ASSERT(string_driver_path.get_type() == Link::Type::usb);
      TEST_ASSERT(string_driver_path.get_driver_name() == "usb");
      TEST_ASSERT(string_driver_path.get_vendor_id() == "0x4100");
      TEST_ASSERT(string_driver_path.get_product_id() == "0x0001");
      TEST_ASSERT(string_driver_path.get_serial_number() == "12345");
      TEST_ASSERT(string_driver_path.get_interface_number() == "0");
      TEST_ASSERT(string_driver_path.get_device_path() == "/dev/serial.tty");
      TEST_ASSERT(string_driver_path.path() == driver_path.path());
    }

    return true;
  }

  bool link_path_case() {

    Link link;

    {
      Link::Path path("host@test.txt", link.driver());
      TEST_ASSERT(path.is_valid() == true);
      TEST_ASSERT(path.is_host_path() == true);
      TEST_ASSERT(path.is_device_path() == false);
      TEST_ASSERT(path.path() == "test.txt");
      TEST_ASSERT(path.driver() == nullptr);
      TEST_ASSERT(path.path_description() == "host@test.txt");
      TEST_ASSERT(path.prefix() == Link::Path::host_prefix());
    }

    {
      Link::Path path("device@/home/test.txt", link.driver());
      TEST_ASSERT(path.is_valid() == true);
      TEST_ASSERT(path.is_host_path() == false);
      TEST_ASSERT(path.is_device_path() == true);
      TEST_ASSERT(path.path() == "/home/test.txt");
      TEST_ASSERT(path.driver() != nullptr);
      TEST_ASSERT(path.path_description() == "device@/home/test.txt");
      TEST_ASSERT(path.prefix() == Link::Path::device_prefix());
    }

    {
      Link::Path path;
      TEST_ASSERT(path.is_valid() == false);
    }

    return true;
  }

private:
};
