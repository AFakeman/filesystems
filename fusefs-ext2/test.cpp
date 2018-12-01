#include <cstring>

#include <errno.h>

#include "prove.hpp"
#include "Ext2Driver.hpp"

const char kTestFile[] = "simple_image.img";

PROVE_CASE(TestInit) {
  Ext2Driver driver(kTestFile);
  driver.Initialize();
}

PROVE_CASE(TestOpen) {
  Ext2Driver driver(kTestFile);
  driver.Initialize();
  int fd = driver.Open("/test");
  char buf[6];
  buf[5] = '\0';
  PROVE_CHECK(driver.Read(fd, buf, sizeof(buf), 0) == sizeof(buf) - 1);
  PROVE_CHECK(std::strcmp("TEST\n", buf) == 0);
  driver.Close(fd);
}

PROVE_CASE(TestNonexistentFile) {
  Ext2Driver driver(kTestFile);
  driver.Initialize();
  bool fired = false;
  try {
    driver.Open("/hello/there/general/kenobi");
  } catch (const std::system_error &err) {
    PROVE_CHECK(err.code().value() == ENOENT);
    fired = true;
  }
  PROVE_CHECK(fired);
}

PROVE_CASE(TestReaddir) {
  Ext2Driver driver(kTestFile);
  driver.Initialize();
  int fd = driver.Opendir("/");
  {
    auto filename = driver.Readdir(fd);
    PROVE_CHECK(filename.has_value());
    PROVE_CHECK(filename.value_or("") == ".");
  }
  {
    auto filename = driver.Readdir(fd);
    PROVE_CHECK(filename.has_value());
    PROVE_CHECK(filename.value_or("") == "..");
  }
  {
    auto filename = driver.Readdir(fd);
    PROVE_CHECK(filename.has_value());
    PROVE_CHECK(filename.value_or("") == "test");
  }
  {
    auto filename = driver.Readdir(fd);
    PROVE_CHECK(filename.has_value());
    PROVE_CHECK(filename.value_or("") == "test2");
  }
  {
    auto filename = driver.Readdir(fd);
    PROVE_CHECK(!filename.has_value());
  }
}

int main() {
  prove::run();
}
