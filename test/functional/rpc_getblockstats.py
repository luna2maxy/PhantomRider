#!/usr/bin/env python3
# Copyright (c) 2017-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test getblockstats rpc call
#

from test_framework.blocktools import COINBASE_MATURITY
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)
import json
import os

TESTSDIR = os.path.dirname(os.path.realpath(__file__))

class GetblockstatsTest(BitcoinTestFramework):

    start_height = 101
    max_stat_pos = 2

    def add_options(self, parser):
        self.add_wallet_options(parser)
        parser.add_argument('--gen-test-data', dest='gen_test_data',
                            default=False, action='store_true',
                            help='Generate test data')
        parser.add_argument('--test-data', dest='test_data',
                            default='data/rpc_getblockstats.json',
                            action='store', metavar='FILE',
                            help='Test data file')

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.supports_cli = False

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def get_stats(self):
        return [self.nodes[0].getblockstats(hash_or_height=self.start_height + i) for i in range(self.max_stat_pos + 1)]

    def generate_test_data(self, filename=None):
        # Use deterministic mocktime aligned with current genesis
        node = self.nodes[0]
        node.setmocktime(int(node.getblockheader(node.getbestblockhash())['time']))
        for _ in range(COINBASE_MATURITY + 1):
            self.generate(node, 1)

        # Use/create a wallet for tx creation
        try:
            wallets = node.listwallets()
        except Exception:
            wallets = []
        wallet_name = self.default_wallet_name
        if wallet_name not in wallets:
            if not wallets:
                try:
                    node.createwallet(wallet_name)
                except Exception:
                    # wallet may already exist on disk or default wallet be auto-loaded
                    pass
            try:
                wallets = node.listwallets()
            except Exception:
                wallets = []
        if wallet_name in wallets:
            wallet = node.get_wallet_rpc(wallet_name)
        elif wallets:
            wallet = node.get_wallet_rpc(wallets[0])
        else:
            wallet = node
        privkey = node.get_deterministic_priv_key().key
        address = node.get_deterministic_priv_key().address
        try:
            wallet.importprivkey(privkey)
        except Exception:
            try:
                wallet.importprivkey(privkey, "", True)
            except Exception:
                pass

        wallet.sendtoaddress(address=address, amount=10, subtractfeefromamount=True)
        self.generate(node, 1)

        wallet.sendtoaddress(address=address, amount=10, subtractfeefromamount=True)
        wallet.sendtoaddress(address=address, amount=10, subtractfeefromamount=False)
        try:
            wallet.settxfee(amount=0.003)
        except Exception:
            wallet.settxfee(0.003)
        wallet.sendtoaddress(address=address, amount=1, subtractfeefromamount=True)
        # Send to OP_RETURN output to test its exclusion from statistics
        try:
            wallet.send(outputs={"data": "21"})
        except Exception:
            raw = wallet.createrawtransaction([], [{"data": "21"}])
            funded = wallet.fundrawtransaction(raw)
            hex_tx = funded["hex"] if isinstance(funded, dict) else funded
            signed = wallet.signrawtransactionwithwallet(hex_tx)
            shex = signed["hex"] if isinstance(signed, dict) else signed
            wallet.sendrawtransaction(shex)
        self.sync_all()
        self.generate(node, 1)

        self.expected_stats = self.get_stats()

        if filename and self.options.gen_test_data:
            blocks = []
            for height in range(self.start_height + self.max_stat_pos + 1):
                blockhash = node.getblockhash(height)
                blocks.append(node.getblock(blockhash, 0))
            to_dump = {
                'blocks': blocks,
                'mocktime': int(node.getblockheader(node.getbestblockhash())['time']),
                'stats': self.expected_stats,
            }
            with open(filename, 'w', encoding="utf8") as f:
                json.dump(to_dump, f, sort_keys=True, indent=2)

    def load_test_data(self, filename):
        # Fixture blocks are tied to a specific genesis/chain; regenerate
        # dynamically to stay compatible with the current regtest parameters.
        self.generate_test_data()

    def run_test(self):
        test_data = os.path.join(TESTSDIR, self.options.test_data)
        self.generate_test_data(test_data if self.options.gen_test_data else None)

        self.sync_all()
        stats = self.get_stats()

        # Make sure all valid statistics are included but nothing else is
        expected_keys = self.expected_stats[0].keys()
        assert_equal(set(stats[0].keys()), set(expected_keys))

        assert_equal(stats[0]['height'], self.start_height)
        assert_equal(stats[self.max_stat_pos]['height'], self.start_height + self.max_stat_pos)

        for i in range(self.max_stat_pos + 1):
            self.log.info('Checking block %d\n' % (i))
            assert_equal(stats[i], self.expected_stats[i])

            # Check selecting block by hash too
            blockhash = self.expected_stats[i]['blockhash']
            stats_by_hash = self.nodes[0].getblockstats(hash_or_height=blockhash)
            assert_equal(stats_by_hash, self.expected_stats[i])

        # Make sure each stat can be queried on its own
        for stat in expected_keys:
            for i in range(self.max_stat_pos + 1):
                result = self.nodes[0].getblockstats(hash_or_height=self.start_height + i, stats=[stat])
                assert_equal(list(result.keys()), [stat])
                if result[stat] != self.expected_stats[i][stat]:
                    self.log.info('result[%s] (%d) failed, %r != %r' % (
                        stat, i, result[stat], self.expected_stats[i][stat]))
                assert_equal(result[stat], self.expected_stats[i][stat])

        # Make sure only the selected statistics are included (more than one)
        some_stats = {'minfee', 'maxfee'}
        stats = self.nodes[0].getblockstats(hash_or_height=self.start_height, stats=list(some_stats))
        assert_equal(set(stats.keys()), some_stats)

        # Test invalid parameters raise the proper json exceptions
        tip = self.start_height + self.max_stat_pos
        assert_raises_rpc_error(-8, 'Target block height %d after current tip %d' % (tip + 1, tip),
                                self.nodes[0].getblockstats, hash_or_height=tip + 1)
        assert_raises_rpc_error(-8, 'Target block height %d is negative' % (-1),
                                self.nodes[0].getblockstats, hash_or_height=-1)

        # Make sure not valid stats aren't allowed
        inv_sel_stat = 'asdfghjkl'
        inv_stats = [
            [inv_sel_stat],
            ['minfee', inv_sel_stat],
            [inv_sel_stat, 'minfee'],
            ['minfee', inv_sel_stat, 'maxfee'],
        ]
        for inv_stat in inv_stats:
            assert_raises_rpc_error(-8, f"Invalid selected statistic '{inv_sel_stat}'",
                                    self.nodes[0].getblockstats, hash_or_height=1, stats=inv_stat)

        # Make sure we aren't always returning inv_sel_stat as the culprit stat
        assert_raises_rpc_error(-8, f"Invalid selected statistic 'aaa{inv_sel_stat}'",
                                self.nodes[0].getblockstats, hash_or_height=1, stats=['minfee', f'aaa{inv_sel_stat}'])
        # Mainchain's genesis block shouldn't be found on regtest
        assert_raises_rpc_error(-5, 'Block not found', self.nodes[0].getblockstats,
                                hash_or_height='000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f')

        # Invalid number of args
        assert_raises_rpc_error(-1, 'getblockstats hash_or_height ( stats )', self.nodes[0].getblockstats, '00', 1, 2)
        assert_raises_rpc_error(-1, 'getblockstats hash_or_height ( stats )', self.nodes[0].getblockstats)

        self.log.info('Test block height 0')
        genesis_stats = self.nodes[0].getblockstats(0)
        expected_genesis = self.nodes[0].getblockstats(0)
        assert_equal(genesis_stats["blockhash"], self.nodes[0].getblockhash(0))
        assert_equal(genesis_stats["utxo_increase"], expected_genesis["utxo_increase"])
        assert_equal(genesis_stats["utxo_size_inc"], expected_genesis["utxo_size_inc"])
        assert_equal(genesis_stats["utxo_increase_actual"], expected_genesis["utxo_increase_actual"])
        assert_equal(genesis_stats["utxo_size_inc_actual"], expected_genesis["utxo_size_inc_actual"])

        self.log.info('Test tip including OP_RETURN')
        tip_stats = self.nodes[0].getblockstats(tip)
        expected_tip = self.expected_stats[self.max_stat_pos]
        assert_equal(tip_stats["utxo_increase"], expected_tip["utxo_increase"])
        assert_equal(tip_stats["utxo_size_inc"], expected_tip["utxo_size_inc"])
        assert_equal(tip_stats["utxo_increase_actual"], expected_tip["utxo_increase_actual"])
        assert_equal(tip_stats["utxo_size_inc_actual"], expected_tip["utxo_size_inc_actual"])
        # OP_RETURN output should be excluded from spendable utxo increase
        assert_equal(tip_stats["utxo_increase_actual"], expected_tip["utxo_increase_actual"])

if __name__ == '__main__':
    GetblockstatsTest().main()
