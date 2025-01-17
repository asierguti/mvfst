/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#include <quic/logging/QLogger.h>

#include <folly/dynamic.h>
#include <quic/codec/Types.h>
#include <quic/logging/QLoggerConstants.h>
#include <quic/logging/QLoggerTypes.h>

namespace quic {

std::unique_ptr<QLogPacketEvent> QLogger::createPacketEvent(
    const RegularQuicPacket& regularPacket,
    uint64_t packetSize) {
  auto event = std::make_unique<QLogPacketEvent>();
  event->refTime = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - refTimePoint);
  event->packetSize = packetSize;
  event->eventType = QLogEventType::PacketReceived;
  event->packetType = folly::variant_match(
      regularPacket.header,
      [](const LongHeader& header) { return toString(header.getHeaderType()); },
      [](const ShortHeader& /* unused*/) {
        return kShortHeaderPacketType.toString();
      });
  if (event->packetType != toString(LongHeader::Types::Retry)) {
    // A Retry packet does not include a packet number.
    event->packetNum = folly::variant_match(
        regularPacket.header,
        [](const auto& h) { return h.getPacketSequenceNum(); });
  }

  uint64_t numPaddingFrames = 0;
  // looping through the packet to store logs created from frames in the packet
  for (const auto& quicFrame : regularPacket.frames) {
    folly::variant_match(
        quicFrame,
        [&](const PaddingFrame& /* unused */) { ++numPaddingFrames; },
        [&](const RstStreamFrame& frame) {
          event->frames.push_back(std::make_unique<RstStreamFrameLog>(
              frame.streamId, frame.errorCode, frame.offset));
        },
        [&](const ConnectionCloseFrame& frame) {
          event->frames.push_back(std::make_unique<ConnectionCloseFrameLog>(
              frame.errorCode, frame.reasonPhrase, frame.closingFrameType));
        },
        [&](const ApplicationCloseFrame& frame) {
          event->frames.push_back(std::make_unique<ApplicationCloseFrameLog>(
              frame.errorCode, frame.reasonPhrase));
        },
        [&](const MaxDataFrame& frame) {
          event->frames.push_back(
              std::make_unique<MaxDataFrameLog>(frame.maximumData));
        },
        [&](const MaxStreamDataFrame& frame) {
          event->frames.push_back(std::make_unique<MaxStreamDataFrameLog>(
              frame.streamId, frame.maximumData));
        },
        [&](const MaxStreamsFrame& frame) {
          event->frames.push_back(std::make_unique<MaxStreamsFrameLog>(
              frame.maxStreams, frame.isForBidirectional));
        },
        [&](const StreamsBlockedFrame& frame) {
          event->frames.push_back(std::make_unique<StreamsBlockedFrameLog>(
              frame.streamLimit, frame.isForBidirectional));
        },
        [&](const PingFrame& /* unused */) {
          event->frames.push_back(std::make_unique<PingFrameLog>());
        },
        [&](const DataBlockedFrame& frame) {
          event->frames.push_back(
              std::make_unique<DataBlockedFrameLog>(frame.dataLimit));
        },
        [&](const StreamDataBlockedFrame& frame) {
          event->frames.push_back(std::make_unique<StreamDataBlockedFrameLog>(
              frame.streamId, frame.dataLimit));
        },
        [&](const WriteAckFrame& frame) {
          event->frames.push_back(std::make_unique<WriteAckFrameLog>(
              frame.ackBlocks, frame.ackDelay));
        },
        [&](const ReadAckFrame& frame) {
          event->frames.push_back(std::make_unique<ReadAckFrameLog>(
              frame.ackBlocks, frame.ackDelay));
        },
        [&](const WriteStreamFrame& frame) {
          event->frames.push_back(std::make_unique<StreamFrameLog>(
              frame.streamId, frame.offset, frame.len, frame.fin));
        },
        [&](const WriteCryptoFrame& frame) {
          event->frames.push_back(
              std::make_unique<CryptoFrameLog>(frame.offset, frame.len));
        },
        [&](const ReadStreamFrame& frame) {
          event->frames.push_back(std::make_unique<StreamFrameLog>(
              frame.streamId, frame.offset, frame.data->length(), frame.fin));
        },
        [&](const ReadCryptoFrame& frame) {
          event->frames.push_back(std::make_unique<CryptoFrameLog>(
              frame.offset, frame.data->length()));
        },
        [&](const ReadNewTokenFrame& /* unused */) {
          event->frames.push_back(std::make_unique<ReadNewTokenFrameLog>());
        },
        [&](const StopSendingFrame& frame) {
          event->frames.push_back(std::make_unique<StopSendingFrameLog>(
              frame.streamId, frame.errorCode));
        },
        [&](const MinStreamDataFrame& frame) {
          event->frames.push_back(std::make_unique<MinStreamDataFrameLog>(
              frame.streamId, frame.maximumData, frame.minimumStreamOffset));
        },
        [&](const ExpiredStreamDataFrame& frame) {
          event->frames.push_back(std::make_unique<ExpiredStreamDataFrameLog>(
              frame.streamId, frame.minimumStreamOffset));
        },
        [&](const PathChallengeFrame& frame) {
          event->frames.push_back(
              std::make_unique<PathChallengeFrameLog>(frame.pathData));
        },
        [&](const PathResponseFrame& frame) {
          event->frames.push_back(
              std::make_unique<PathResponseFrameLog>(frame.pathData));
        },
        [&](const NewConnectionIdFrame& frame) {
          event->frames.push_back(std::make_unique<NewConnectionIdFrameLog>(
              frame.sequenceNumber, frame.token));
        },
        [&](const auto& /* unused */) {
          // Ignore other frames.
        });
  }
  if (numPaddingFrames > 0) {
    event->frames.push_back(
        std::make_unique<PaddingFrameLog>(numPaddingFrames));
  }
  return event;
}

std::unique_ptr<QLogPacketEvent> QLogger::createPacketEvent(
    const RegularQuicWritePacket& writePacket,
    uint64_t packetSize) {
  auto event = std::make_unique<QLogPacketEvent>();
  event->refTime = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - refTimePoint);
  event->packetNum = folly::variant_match(
      writePacket.header,
      [](const auto& h) { return h.getPacketSequenceNum(); });
  event->packetSize = packetSize;
  event->eventType = QLogEventType::PacketSent;
  event->packetType = folly::variant_match(
      writePacket.header,
      [](const LongHeader& header) { return toString(header.getHeaderType()); },
      [](const ShortHeader& /* unused*/) {
        return kShortHeaderPacketType.toString();
      });

  uint64_t numPaddingFrames = 0;
  // looping through the packet to store logs created from frames in the packet
  for (const auto& quicFrame : writePacket.frames) {
    folly::variant_match(
        quicFrame,
        [&](const PaddingFrame& /* unused */) { ++numPaddingFrames; },
        [&](const RstStreamFrame& frame) {
          event->frames.push_back(std::make_unique<RstStreamFrameLog>(
              frame.streamId, frame.errorCode, frame.offset));
        },
        [&](const ConnectionCloseFrame& frame) {
          event->frames.push_back(std::make_unique<ConnectionCloseFrameLog>(
              frame.errorCode, frame.reasonPhrase, frame.closingFrameType));
        },
        [&](const ApplicationCloseFrame& frame) {
          event->frames.push_back(std::make_unique<ApplicationCloseFrameLog>(
              frame.errorCode, frame.reasonPhrase));
        },
        [&](const MaxDataFrame& frame) {
          event->frames.push_back(
              std::make_unique<MaxDataFrameLog>(frame.maximumData));
        },
        [&](const MaxStreamDataFrame& frame) {
          event->frames.push_back(std::make_unique<MaxStreamDataFrameLog>(
              frame.streamId, frame.maximumData));
        },
        [&](const MaxStreamsFrame& frame) {
          event->frames.push_back(std::make_unique<MaxStreamsFrameLog>(
              frame.maxStreams, frame.isForBidirectional));
        },
        [&](const StreamsBlockedFrame& frame) {
          event->frames.push_back(std::make_unique<StreamsBlockedFrameLog>(
              frame.streamLimit, frame.isForBidirectional));
        },
        [&](const PingFrame& /* unused */) {
          event->frames.push_back(std::make_unique<PingFrameLog>());
        },
        [&](const DataBlockedFrame& frame) {
          event->frames.push_back(
              std::make_unique<DataBlockedFrameLog>(frame.dataLimit));
        },
        [&](const StreamDataBlockedFrame& frame) {
          event->frames.push_back(std::make_unique<StreamDataBlockedFrameLog>(
              frame.streamId, frame.dataLimit));
        },
        [&](const WriteAckFrame& frame) {
          event->frames.push_back(std::make_unique<WriteAckFrameLog>(
              frame.ackBlocks, frame.ackDelay));
        },
        [&](const ReadAckFrame& frame) {
          event->frames.push_back(std::make_unique<ReadAckFrameLog>(
              frame.ackBlocks, frame.ackDelay));
        },
        [&](const WriteStreamFrame& frame) {
          event->frames.push_back(std::make_unique<StreamFrameLog>(
              frame.streamId, frame.offset, frame.len, frame.fin));
        },
        [&](const WriteCryptoFrame& frame) {
          event->frames.push_back(
              std::make_unique<CryptoFrameLog>(frame.offset, frame.len));
        },
        [&](const ReadStreamFrame& frame) {
          event->frames.push_back(std::make_unique<StreamFrameLog>(
              frame.streamId, frame.offset, frame.data->length(), frame.fin));
        },
        [&](const ReadCryptoFrame& frame) {
          event->frames.push_back(std::make_unique<CryptoFrameLog>(
              frame.offset, frame.data->length()));
        },
        [&](const ReadNewTokenFrame& /* unused */) {
          event->frames.push_back(std::make_unique<ReadNewTokenFrameLog>());
        },
        [&](const StopSendingFrame& frame) {
          event->frames.push_back(std::make_unique<StopSendingFrameLog>(
              frame.streamId, frame.errorCode));
        },
        [&](const MinStreamDataFrame& frame) {
          event->frames.push_back(std::make_unique<MinStreamDataFrameLog>(
              frame.streamId, frame.maximumData, frame.minimumStreamOffset));
        },
        [&](const ExpiredStreamDataFrame& frame) {
          event->frames.push_back(std::make_unique<ExpiredStreamDataFrameLog>(
              frame.streamId, frame.minimumStreamOffset));
        },
        [&](const PathChallengeFrame& frame) {
          event->frames.push_back(
              std::make_unique<PathChallengeFrameLog>(frame.pathData));
        },
        [&](const PathResponseFrame& frame) {
          event->frames.push_back(
              std::make_unique<PathResponseFrameLog>(frame.pathData));
        },
        [&](const NewConnectionIdFrame& frame) {
          event->frames.push_back(std::make_unique<NewConnectionIdFrameLog>(
              frame.sequenceNumber, frame.token));
        },
        [&](const auto& /* unused */) {
          // Ignore other frames.
        });
  }
  if (numPaddingFrames > 0) {
    event->frames.push_back(
        std::make_unique<PaddingFrameLog>(numPaddingFrames));
  }
  return event;
}

std::unique_ptr<QLogVersionNegotiationEvent> QLogger::createPacketEvent(
    const VersionNegotiationPacket& versionPacket,
    uint64_t packetSize,
    bool isPacketRecvd) {
  auto event = std::make_unique<QLogVersionNegotiationEvent>();
  event->refTime = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - refTimePoint);
  event->packetSize = packetSize;
  event->eventType =
      isPacketRecvd ? QLogEventType::PacketReceived : QLogEventType::PacketSent;
  event->packetType = kVersionNegotiationPacketType;
  event->versionLog = std::make_unique<VersionNegotiationLog>(
      VersionNegotiationLog(versionPacket.versions));
  return event;
}

std::string getFlowControlEvent(int offset) {
  return "flow control event, new offset: " + folly::to<std::string>(offset);
};

std::string
getRxStreamWU(StreamId streamId, PacketNum packetNum, uint64_t maximumData) {
  return "rx stream, streamId: " + folly::to<std::string>(streamId) +
      ", packetNum: " + folly::to<std::string>(packetNum) +
      ", maximumData: " + folly::to<std::string>(maximumData);
};

std::string getRxConnWU(PacketNum packetNum, uint64_t maximumData) {
  return "rx, packetNum: " + folly::to<std::string>(packetNum) +
      ", maximumData: " + folly::to<std::string>(maximumData);
};

std::string getPeerClose(const std::string& peerCloseReason) {
  return "error message: " + peerCloseReason;
};

std::string getFlowControlWindowAvailable(uint64_t windowAvailable) {
  return "on flow control, window available: " +
      folly::to<std::string>(windowAvailable);
};

std::string getClosingStream(const std::string& streamId) {
  return "closing stream, stream id: " + streamId;
};

} // namespace quic
