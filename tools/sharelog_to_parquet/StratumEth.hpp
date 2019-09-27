/*
 The MIT License (MIT)

 Copyright (c) [2016] [BTC.COM]

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */
#pragma once

#include <string>
#include <cmath>
#include <glog/logging.h>

#include "eth/eth.pb.h"
#include "StratumStatus.h"
#include "uint256.h"
#include "arith_uint256.h"

using namespace std;

class EthDifficulty {
public:
  // Bitcoin-style bits are compression of the target
  static double BitcoinStyleBitsToDifficulty(uint32_t bitcoinStyleBits) {
    static arith_uint256 kMaxUint256(
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    if (bitcoinStyleBits == 0) {
      return 0.0;
    }

    // The previous sharelog stored arith_uint256::bits().
    if (bitcoinStyleBits <= 0xffL) {
      arith_uint256 target = (arith_uint256("1") << bitcoinStyleBits) - 1;
      return (kMaxUint256 / target).getdouble();
    }

    // The new sharelog will store arith_uint256::GetCompact() to improve
    // precision.
    arith_uint256 target;
    target.SetCompact(bitcoinStyleBits);
    return (kMaxUint256 / target).getdouble();
  }
};

class ShareEthBytesVersion {
public:
  uint32_t version_ = 0; // 0
  uint32_t checkSum_ = 0; // 4

  int64_t workerHashId_ = 0; // 8
  int32_t userId_ = 0; // 16
  int32_t status_ = 0; // 20
  int64_t timestamp_ = 0; // 24
  IpAddress ip_ = 0; // 32

  uint64_t headerHash_ = 0; // 48
  uint64_t shareDiff_ = 0; // 56
  uint64_t networkDiff_ = 0; // 64
  uint64_t nonce_ = 0; // 72
  uint32_t sessionId_ = 0; // 80
  uint32_t height_ = 0; // 84

  uint32_t checkSum() const {
    uint64_t c = 0;

    c += (uint64_t)version_;
    c += (uint64_t)workerHashId_;
    c += (uint64_t)userId_;
    c += (uint64_t)status_;
    c += (uint64_t)timestamp_;
    c += (uint64_t)ip_.addrUint64[0];
    c += (uint64_t)ip_.addrUint64[1];
    c += (uint64_t)headerHash_;
    c += (uint64_t)shareDiff_;
    c += (uint64_t)networkDiff_;
    c += (uint64_t)nonce_;
    c += (uint64_t)sessionId_;
    c += (uint64_t)height_;

    return ((uint32_t)c) + ((uint32_t)(c >> 32));
  }
};

class ShareEth : public sharebase::EthMsg {
public:
  const static uint32_t CURRENT_VERSION_FOUNDATION =
      0x00110003u; // first 0011: ETH, second 0002: version 3
  const static uint32_t CURRENT_VERSION_CLASSIC =
      0x00160003u; // first 0016: ETC, second 0002: version 3
  const static uint32_t BYTES_VERSION_FOUNDATION =
      0x00110002u; // first 0011: ETH, second 0002: version 3
  const static uint32_t BYTES_VERSION_CLASSIC =
      0x00160002u; // first 0016: ETC, second 0002: version 3

  ShareEth() {
    set_version(0);
    set_workerhashid(0);
    set_userid(0);
    set_status(0);
    set_timestamp(0);
    set_ip("0.0.0.0");
    set_headerhash(0);
    set_sharediff(0);
    set_networkdiff(0);
    set_height(0);
    set_nonce(0);
    set_sessionid(0);
  }
  ShareEth(const ShareEth &r) = default;
  ShareEth &operator=(const ShareEth &r) = default;

  bool SerializeToBuffer(string &data, uint32_t &size) const {
    size = ByteSize();
    data.resize(size);

    if (!SerializeToArray((uint8_t *)data.data(), size)) {
      DLOG(INFO) << "base.SerializeToArray failed!" << std::endl;
      return false;
    }

    return true;
  }

  bool UnserializeWithVersion(const uint8_t *data, uint32_t size) {

    if (nullptr == data || size <= 0) {
      return false;
    }

    const uint8_t *payload = data;
    uint32_t version = *((uint32_t *)payload);

    if (version == CURRENT_VERSION_FOUNDATION ||
        version == CURRENT_VERSION_CLASSIC) {

      if (!ParseFromArray(
              (const uint8_t *)(payload + sizeof(uint32_t)),
              size - sizeof(uint32_t))) {
        DLOG(INFO) << "share ParseFromArray failed!";
        return false;
      }
    } else if (
        (version == BYTES_VERSION_FOUNDATION ||
         version == BYTES_VERSION_CLASSIC) &&
        size == sizeof(ShareEthBytesVersion)) {

      ShareEthBytesVersion *share = (ShareEthBytesVersion *)payload;

      if (share->checkSum() != share->checkSum_) {
        DLOG(INFO) << "checkSum mismatched! checkSum_: " << share->checkSum_
                   << ", checkSum(): " << share->checkSum();
        return false;
      }

      set_version(
          share->version_ == BYTES_VERSION_CLASSIC
              ? CURRENT_VERSION_CLASSIC
              : CURRENT_VERSION_FOUNDATION);
      set_workerhashid(share->workerHashId_);
      set_userid(share->userId_);
      set_status(share->status_);
      set_timestamp(share->timestamp_);
      set_ip(share->ip_.toString());
      set_headerhash(share->headerHash_);
      set_sharediff(share->shareDiff_);
      set_networkdiff(share->networkDiff_);
      set_nonce(share->nonce_);
      set_sessionid(share->sessionId_);
      set_height(share->height_);

    } else {

      DLOG(INFO) << "unknow share received! data size: " << size;
      return false;
    }

    return true;
  }

  bool SerializeToArrayWithLength(string &data, uint32_t &size) const {
    size = ByteSize();
    data.resize(size + sizeof(uint32_t));

    *((uint32_t *)data.data()) = size;
    uint8_t *payload = (uint8_t *)data.data();

    if (!SerializeToArray(payload + sizeof(uint32_t), size)) {
      DLOG(INFO) << "base.SerializeToArray failed!";
      return false;
    }

    size += sizeof(uint32_t);
    return true;
  }

  bool SerializeToArrayWithVersion(string &data, uint32_t &size) const {
    size = ByteSize();
    data.resize(size + sizeof(uint32_t));

    uint8_t *payload = (uint8_t *)data.data();
    *((uint32_t *)payload) = version();

    if (!SerializeToArray(payload + sizeof(uint32_t), size)) {
      DLOG(INFO) << "SerializeToArray failed!";
      return false;
    }

    size += sizeof(uint32_t);
    return true;
  }

  size_t getsharelength() { return IsInitialized() ? ByteSize() : 0; }
};

//----------------------------------------------------

template <>
class ParquetWriterT<ShareEth> : public ParquetWriter {
protected:
  int64_t *indexs_ = nullptr;
  int64_t *workerIds_ = nullptr;
  int32_t *userIds_ = nullptr;
  int32_t *status_ = nullptr;
  int64_t *timestamps_ = nullptr;
  parquet::ByteArray *ip_ = nullptr;
  std::string *ipStr_ = nullptr;
  int64_t *jobIds_ = nullptr;
  int64_t *shareDiff_ = nullptr;
  double *networkDiff_ = nullptr;
  int32_t *height_ = nullptr;
  int64_t *nonce_ = nullptr;
  int32_t *sessionId_ = nullptr;
  int32_t *outputHash_ = nullptr;
  int32_t *extUserId_ = nullptr;
  double *diffReached_ = nullptr;

public:
  ParquetWriterT() {
    indexs_ = new int64_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    workerIds_ = new int64_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    userIds_ = new int32_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    status_ = new int32_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    timestamps_ = new int64_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    ip_ = new parquet::ByteArray[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    ipStr_ = new std::string[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    jobIds_ = new int64_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    shareDiff_ = new int64_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    networkDiff_ = new double[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    height_ = new int32_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    nonce_ = new int64_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    sessionId_ = new int32_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    extUserId_ = new int32_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    diffReached_ = new double[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
  }

  ~ParquetWriterT() {
    if (shareNum_ > 0) {
      flushShares();
    }

    if (indexs_)
      delete[] indexs_;
    if (workerIds_)
      delete[] workerIds_;
    if (userIds_)
      delete[] userIds_;
    if (status_)
      delete[] status_;
    if (timestamps_)
      delete[] timestamps_;
    if (ip_)
      delete[] ip_;
    if (ipStr_)
      delete[] ipStr_;
    if (jobIds_)
      delete[] jobIds_;
    if (shareDiff_)
      delete[] shareDiff_;
    if (networkDiff_)
      delete[] networkDiff_;
    if (height_)
      delete[] height_;
    if (nonce_)
      delete[] nonce_;
    if (sessionId_)
      delete[] sessionId_;
    if (extUserId_)
      delete[] extUserId_;
    if (diffReached_)
      delete[] diffReached_;
  }

protected:
  std::shared_ptr<GroupNode> setupSchema() override {
    parquet::schema::NodeVector fields;

    fields.push_back(
        PrimitiveNode::Make("index", Repetition::REQUIRED, Type::INT64));
    fields.push_back(
        PrimitiveNode::Make("worker_id", Repetition::REQUIRED, Type::INT64));
    fields.push_back(
        PrimitiveNode::Make("user_id", Repetition::REQUIRED, Type::INT32));
    fields.push_back(
        PrimitiveNode::Make("status", Repetition::REQUIRED, Type::INT32));
    fields.push_back(
        PrimitiveNode::Make("timestamp", Repetition::REQUIRED, Type::INT64));
    fields.push_back(PrimitiveNode::Make(
        "ip", Repetition::REQUIRED, Type::BYTE_ARRAY, LogicalType::UTF8));
    fields.push_back(
        PrimitiveNode::Make("job_id", Repetition::REQUIRED, Type::INT64));
    fields.push_back(
        PrimitiveNode::Make("share_diff", Repetition::REQUIRED, Type::INT64));
    fields.push_back(PrimitiveNode::Make(
        "network_diff", Repetition::REQUIRED, Type::DOUBLE));
    fields.push_back(
        PrimitiveNode::Make("height", Repetition::REQUIRED, Type::INT32));
    fields.push_back(
        PrimitiveNode::Make("nonce", Repetition::REQUIRED, Type::INT64));
    fields.push_back(
        PrimitiveNode::Make("session_id", Repetition::REQUIRED, Type::INT32));
    fields.push_back(
        PrimitiveNode::Make("ext_user_id", Repetition::REQUIRED, Type::INT32));
    fields.push_back(PrimitiveNode::Make(
        "diff_reached", Repetition::REQUIRED, Type::DOUBLE));

    // Create a GroupNode named 'share_bitcoin' using the primitive nodes
    // defined above This GroupNode is the root node of the schema tree
    return std::static_pointer_cast<GroupNode>(
        GroupNode::Make("share_eth", Repetition::REQUIRED, fields));
  }

  void flushShares() {
    DLOG(INFO) << "flush " << shareNum_ << " shares";

    // Create a RowGroupWriter instance
    auto rgWriter = fileWriter_->AppendRowGroup();

    // index
    static_cast<parquet::Int64Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, indexs_);

    // worker_id
    static_cast<parquet::Int64Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, workerIds_);

    // user_id
    static_cast<parquet::Int32Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, userIds_);

    // status
    static_cast<parquet::Int32Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, status_);

    // timestamp
    static_cast<parquet::Int64Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, timestamps_);

    // ip
    static_cast<parquet::ByteArrayWriter *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, ip_);

    // job_id
    static_cast<parquet::Int64Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, jobIds_);

    // share_diff
    static_cast<parquet::Int64Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, shareDiff_);

    // network_diff
    static_cast<parquet::DoubleWriter *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, networkDiff_);

    // height
    static_cast<parquet::Int32Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, height_);

    // nonce
    static_cast<parquet::Int64Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, nonce_);

    // session_id
    static_cast<parquet::Int32Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, sessionId_);

    // ext_user_id
    static_cast<parquet::Int32Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, extUserId_);

    // diff_reached
    static_cast<parquet::DoubleWriter *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, diffReached_);

    // Save current RowGroup
    rgWriter->Close();

    shareNum_ = 0;
  }

public:
  void addShare(const ShareEth &share) {
    static IpAddress ipAddr;
    ipAddr.fromString(share.ip());

    indexs_[shareNum_] = ++index_;
    workerIds_[shareNum_] = share.workerhashid();
    userIds_[shareNum_] = share.userid();
    status_[shareNum_] = share.status();
    timestamps_[shareNum_] = share.timestamp();
    ipStr_[shareNum_] = share.ip();
    ip_[shareNum_].len = ipStr_[shareNum_].size();
    ip_[shareNum_].ptr = (const uint8_t *)ipStr_[shareNum_].data();
    jobIds_[shareNum_] = share.headerhash();
    shareDiff_[shareNum_] = share.sharediff();
    networkDiff_[shareNum_] = share.networkdiff();
    height_[shareNum_] = share.height();
    nonce_[shareNum_] = share.nonce();
    sessionId_[shareNum_] = share.sessionid();
    extUserId_[shareNum_] = share.extuserid();
    diffReached_[shareNum_] =
        EthDifficulty::BitcoinStyleBitsToDifficulty(share.bitsreached());

    shareNum_++;

    if (shareNum_ >= DEFAULT_NUM_ROWS_PER_ROW_GROUP) {
      flushShares();
    }
  }
};
