/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <vector>

#include "common/Constants.h"
#include "common/MessageNames.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libDirectoryService/DirectoryService.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/UpgradeManager.h"

#define BOOST_TEST_MODULE coinbase
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace TestUtils;

BOOST_AUTO_TEST_SUITE(test_coinbase)

BOOST_AUTO_TEST_CASE(test_coinbase_correctness) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  Mediator mediator(GenerateRandomKeyPair(), GenerateRandomPeer());
  DirectoryService dummyDS(mediator);
  Node dummyNode(mediator, 0, true);  // Unused in code anyways
  Lookup dummyLookup(mediator);
  const uint64_t nonce{0};
  auto dummyValidator = make_shared<Validator>(mediator);
  AccountStore::GetInstance().Init();

  AccountStore::GetInstance().AddAccount(Address(),
                                         {TOTAL_COINBASE_REWARD, nonce});
  AccountStore::GetInstance().UpdateStateTrieAll();

  const uint32_t min_ds_size = 600;
  const uint32_t min_comm_size = 100;
  // 2 shards

  mediator.RegisterColleagues(&dummyDS, &dummyNode, &dummyLookup,
                              dummyValidator.get());

  auto dummy_shard_size = (DistUint8() % min_comm_size) + min_comm_size;

  auto dummy_ds_size = DistUint8() % min_ds_size + min_ds_size;
  auto dummy_shards = GenerateDequeueOfShard(dummy_shard_size);

  auto dummy_ds_comm = GenerateRandomDSCommittee(dummy_ds_size);

  auto random_epoch_num = DistUint64();

  vector<bool> b1, b2;

  int i = 0;
  for (const auto& shard : dummy_shards) {
    b1 = GenerateRandomBooleanVector(shard.size());

    b2 = GenerateRandomBooleanVector(shard.size());
    dummyDS.SaveCoinbaseCore(b1, b2, shard, i++, random_epoch_num);
  }

  b1 = GenerateRandomBooleanVector(dummy_ds_comm.size());
  b2 = GenerateRandomBooleanVector(dummy_ds_comm.size());

  dummyDS.SaveCoinbaseCore(b1, b2, dummy_ds_comm,
                           CoinbaseReward::FINALBLOCK_REWARD, random_epoch_num);
  *mediator.m_DSCommittee = dummy_ds_comm;
  dummyDS.m_shards = dummy_shards;

  dummyDS.InitCoinbase();

  AccountStore::GetInstance().SerializeDelta();

  AccountStore::GetInstance().CommitTempRevertible();

  boost::multiprecision::uint128_t totalReward = 0;

  auto calcReward = [&totalReward](const auto& shard) {
    for (const auto& shardMember : shard) {
      const auto& pubKey = std::get<SHARD_NODE_PUBKEY>(shardMember);
      const auto& address = Account::GetAddressFromPublicKey(pubKey);
      const Account* account = AccountStore::GetInstance().GetAccount(address);
      if (account == nullptr) {
        LOG_GENERAL(WARNING, pubKey << " " << address);
        continue;
      }
      totalReward += account->GetBalance();
    }
  };

  calcReward(dummy_ds_comm);
  for (const auto& shard : dummy_shards) {
    calcReward(shard);
  }

  LOG_GENERAL(INFO, (totalReward * 100) / COINBASE_REWARD_PER_DS
                        << " " << 100 - LOOKUP_REWARD_IN_PERCENT);
}

BOOST_AUTO_TEST_SUITE_END()