
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

  bool link_case() {

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
