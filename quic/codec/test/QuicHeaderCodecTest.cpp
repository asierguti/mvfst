/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#include <folly/Optional.h>
#include <folly/Overload.h>
#include <folly/portability/GTest.h>

#include <quic/QuicException.h>
#include <quic/codec/QuicHeaderCodec.h>
#include <quic/common/test/TestUtils.h>

using namespace testing;
using namespace folly;

namespace quic {
namespace test {

class QuicHeaderCodecTest : public Test {};

TEST_F(QuicHeaderCodecTest, EmptyBuffer) {
  auto emptyBuffer = folly::IOBuf::create(0);
  EXPECT_FALSE(parseHeader(*emptyBuffer).hasValue());
}

TEST_F(QuicHeaderCodecTest, TooSmallBuffer) {
  auto smallBuffer = folly::IOBuf::create(1);
  smallBuffer->append(1);
  folly::io::RWPrivateCursor wcursor(smallBuffer.get());
  wcursor.writeBE<uint8_t>(0x01);
  EXPECT_FALSE(parseHeader(*smallBuffer).hasValue());
}

TEST_F(QuicHeaderCodecTest, VersionNegotiationPacketTest) {
  ConnectionId srcConnId = getTestConnectionId(0),
               destConnId = getTestConnectionId(1);
  QuicVersion version = MVFST1;
  std::vector<QuicVersion> versions{version};
  VersionNegotiationPacketBuilder builder(srcConnId, destConnId, versions);
  auto packet = std::move(builder).buildPacket();
  auto result = parseHeader(*packet.second);
  EXPECT_TRUE(result->isVersionNegotiation);
}

TEST_F(QuicHeaderCodecTest, ShortHeaderTest) {
  PacketNum packetNum = 1;
  RegularQuicPacketBuilder builder(
      kDefaultUDPSendPacketLen,
      ShortHeader(
          ProtectionType::KeyPhaseZero, getTestConnectionId(), packetNum),
      0 /* largestAcked */);
  auto packet = std::move(builder).buildPacket();
  auto result = parseHeader(*packet.header);
  auto& header = result->parsedHeader;

  EXPECT_EQ(
      getTestConnectionId(),
      folly::variant_match(
          header.value(),
          [](const LongHeader& longHeader) {
            return longHeader.getDestinationConnId();
          },
          [](const ShortHeader& shortHeader) {
            return shortHeader.getConnectionId();
          }));
}
} // namespace test
} // namespace quic
