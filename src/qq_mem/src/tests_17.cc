#include "catch.hpp"

#include <iostream>

#include "bloom_filter.h"


TEST_CASE( "Bloom filter store", "[bloomfilter]" ) {
  SECTION("Bloom filter check") {
    BloomFilterStore store(0.00001);
    store.Add(33, {"hello"}, {"world you"});
    FilterCases cases = store.Lookup("hello");
    
    REQUIRE(cases.size() == 1);
    REQUIRE(cases[0].doc_id == 33);

    REQUIRE(cases[0].blm.Check("world") == BLM_MAY_PRESENT);
    REQUIRE(cases[0].blm.Check("you") == BLM_MAY_PRESENT);
    REQUIRE(cases[0].blm.Check("yeu") == BLM_NOT_PRESENT);
    REQUIRE(cases[0].blm.Check("yew") == BLM_NOT_PRESENT);

    SECTION("Serialize and Deserialize") {
      std::string data = cases[0].blm.Serialize();
      BloomFilter blm;
      blm.Deserialize(data.data());

      // REQUIRE(blm.Check("world") == BLM_MAY_PRESENT);
      // REQUIRE(blm.Check("you") == BLM_MAY_PRESENT);
      // REQUIRE(blm.Check("yeu") == BLM_NOT_PRESENT);
      // REQUIRE(blm.Check("yew") == BLM_NOT_PRESENT);
    }
  }

}

void CheckFloat(const float a) {
  std::string data = utils::SerializeFloat(a);
  float b = utils::DeserializeFloat(data.data());

  REQUIRE(a == b);
}

TEST_CASE( "Serializing float", "[utils]" ) {
  CheckFloat(0.1);
  CheckFloat(100.1);
  CheckFloat(100.103);
  CheckFloat(0.7803);
  CheckFloat(0.007803);
}







