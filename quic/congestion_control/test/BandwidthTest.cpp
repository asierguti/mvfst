/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

// Copyright 2004-present Facebook.  All rights reserved.

#include <folly/portability/GTest.h>
#include <quic/congestion_control/Bbr.h>

using namespace testing;

namespace quic {
namespace test {

class BandwidthTest : public Test {};

TEST_F(BandwidthTest, DefaultZero) {
  Bandwidth defaultBandwidth;
  EXPECT_FALSE(defaultBandwidth);
  EXPECT_EQ(0, defaultBandwidth.bytes);
  EXPECT_TRUE(defaultBandwidth == Bandwidth(0, 100us));
  EXPECT_TRUE(Bandwidth(0, 100us) == Bandwidth(0, 200us));
  EXPECT_TRUE(Bandwidth(0, 1us) < Bandwidth(1, 1000us));
}

TEST_F(BandwidthTest, Compare) {
  Bandwidth lowBandwidth(1000, 100us);
  Bandwidth midBandwidth(2000, 150us);
  Bandwidth highBandwidth(4000, 200us);
  EXPECT_TRUE(lowBandwidth < midBandwidth);
  EXPECT_TRUE(highBandwidth > midBandwidth);
  Bandwidth alsoLowBandwidth(2000, 200us);
  EXPECT_TRUE(lowBandwidth == alsoLowBandwidth);
  EXPECT_TRUE(Bandwidth(1500, 150us) > Bandwidth(700, 100us));
  EXPECT_TRUE(Bandwidth(1500, 150us) >= Bandwidth(700, 100us));
  EXPECT_TRUE(Bandwidth(700, 100us) < Bandwidth(1500, 150us));
  EXPECT_TRUE(Bandwidth(700, 100us) <= Bandwidth(1500, 150us));
  EXPECT_TRUE(Bandwidth(700, 100us) <= Bandwidth(1400, 200us));
  EXPECT_FALSE(Bandwidth(700, 100us) == Bandwidth(701, 100us));
  EXPECT_FALSE(Bandwidth(1, 1us) == Bandwidth());
}

TEST_F(BandwidthTest, Arithmetics) {
  Bandwidth testBandwidth(1000, 10us);
  EXPECT_TRUE(testBandwidth);
  Bandwidth zeroBandwidth;
  EXPECT_FALSE(zeroBandwidth);
  EXPECT_EQ(0, zeroBandwidth * 20us);
  std::chrono::microseconds longRtt(20), shortRtt(5);
  EXPECT_EQ(500, testBandwidth * shortRtt);
  EXPECT_EQ(2000, testBandwidth * longRtt);
  EXPECT_EQ(4000, testBandwidth * 2 * longRtt);
  EXPECT_EQ(1000, testBandwidth / 2 * longRtt);
  EXPECT_EQ(750, testBandwidth * 1.5 * shortRtt);
  EXPECT_EQ(666, testBandwidth / 3 * longRtt);
}
} // namespace test
} // namespace quic
