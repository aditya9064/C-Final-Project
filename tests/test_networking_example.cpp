// Example/scaffold tests for the ENet dependency. These exist to prove the
// test harness works end-to-end on a fresh clone; real gameplay tests
// (movement sync, hit detection, disconnect handling, etc.) will replace
// and extend these as each subsystem lands. See docs/proposal.md section 6
// for the full test plan.
#include <doctest/doctest.h>
#include <enet/enet.h>

TEST_CASE("ENet library initializes and shuts down cleanly") {
  // Normal use case.
  CHECK(enet_initialize() == 0);
  enet_deinitialize();
}

TEST_CASE("ENet rejects a malformed IP address") {
  // Invalid-input case: an out-of-range IP should fail to parse rather
  // than silently producing a bogus address.
  REQUIRE(enet_initialize() == 0);

  ENetAddress address{};
  int result = enet_address_set_host_ip(&address, "999.999.999.999");
  CHECK(result != 0);

  enet_deinitialize();
}

TEST_CASE("ENet accepts the loopback address") {
  // Boundary case: the smallest "real" address we rely on for local
  // testing between two clients on one machine.
  REQUIRE(enet_initialize() == 0);

  ENetAddress address{};
  int result = enet_address_set_host_ip(&address, "127.0.0.1");
  CHECK(result == 0);

  enet_deinitialize();
}
