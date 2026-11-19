#!/usr/bin/env python3
"""Regtest full-node validation for the PHR fresh-chain reset.

Exercises genesis, mining, block validation, restart and two-node sync using
the real dashd/dash-cli binaries. Usage:

    python3 test/phr_audit/node_validation.py \
        --dashd build/src/dashd --dash-cli build/src/dash-cli
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

GENESIS = "403ac5ae6a06f8d677cce9fd5cf2bfef5dde8378f4126154be1d30f830e51077"
RPC_USER = "phraudit"
RPC_PASSWORD = "phraudit"
PORT1, RPC1 = 21899, 21898
PORT2, RPC2 = 21900, 21901


class Node:
    def __init__(self, dashd, dash_cli, datadir, port, rpcport, extra=None):
        self.dashd = dashd
        self.dash_cli = dash_cli
        self.datadir = Path(datadir)
        self.port = port
        self.rpcport = rpcport
        self.extra = extra or []
        self.proc = None
        self.log = None

    def args(self):
        return [
            "-regtest", f"-datadir={self.datadir}", f"-port={self.port}",
            f"-rpcport={self.rpcport}", f"-rpcuser={RPC_USER}",
            f"-rpcpassword={RPC_PASSWORD}", "-server=1", "-listen=1",
            "-printtoconsole", "-txindex=0", *self.extra,
        ]

    def start(self):
        self.datadir.mkdir(parents=True, exist_ok=True)
        self.log = open(self.datadir / "phr_node.log", "w")
        self.proc = subprocess.Popen([self.dashd, *self.args()], stdout=self.log, stderr=subprocess.STDOUT)
        for _ in range(100):
            if self.proc.poll() is not None:
                raise RuntimeError(f"dashd exited early: {self.datadir}")
            try:
                self.cli("getblockchaininfo")
                return
            except subprocess.CalledProcessError:
                time.sleep(0.2)
        raise RuntimeError(f"dashd did not become ready: {self.datadir}")

    def cli(self, *args):
        cmd = [
            self.dash_cli, "-regtest", f"-datadir={self.datadir}",
            f"-rpcport={self.rpcport}", f"-rpcuser={RPC_USER}",
            f"-rpcpassword={RPC_PASSWORD}", *(str(a) for a in args),
        ]
        out = subprocess.check_output(cmd, text=True, stderr=subprocess.PIPE)
        try:
            return json.loads(out)
        except json.JSONDecodeError:
            return out.strip()

    def stop(self):
        if self.proc is None:
            return
        try:
            self.cli("stop")
        except subprocess.CalledProcessError:
            pass
        try:
            self.proc.wait(timeout=30)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()
        if self.log:
            self.log.close()
        self.proc = None


def wait_for_height(node, height, timeout=60):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if node.cli("getblockchaininfo")["blocks"] >= height:
            return True
        time.sleep(0.25)
    return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dashd", required=True)
    parser.add_argument("--dash-cli", required=True)
    parser.add_argument("--blocks", type=int, default=12)
    args = parser.parse_args()

    dashd = str(Path(args.dashd).resolve())
    dash_cli = str(Path(args.dash_cli).resolve())
    if not os.access(dashd, os.X_OK) or not os.access(dash_cli, os.X_OK):
        print(f"missing executable: dashd={dashd} dash-cli={dash_cli}", file=sys.stderr)
        return 2

    failures = []
    with tempfile.TemporaryDirectory(prefix="phr-node-") as tmp:
        node1 = Node(dashd, dash_cli, Path(tmp) / "node1", PORT1, RPC1)
        node2 = Node(dashd, dash_cli, Path(tmp) / "node2", PORT2, RPC2,
                     [f"-connect=127.0.0.1:{PORT1}"])
        try:
            node1.start()
            info0 = node1.cli("getblockchaininfo")
            genesis = node1.cli("getblockhash", 0)
            print(f"initial_chain\tchain={info0['chain']}\tblocks={info0['blocks']}\tgenesis={genesis}")
            if genesis != GENESIS:
                failures.append("genesis hash mismatch")
            if info0["chain"] != "regtest":
                failures.append("chain is not regtest")

            # Dash Core does not create a default wallet automatically.
            node1.cli("createwallet", "phr_audit")
            address = node1.cli("getnewaddress")
            mined = node1.cli("generatetoaddress", args.blocks, address)
            info1 = node1.cli("getblockchaininfo")
            tip1 = node1.cli("getbestblockhash")
            print(f"mined\tblocks_requested={args.blocks}\tblocks_returned={len(mined)}"
                  f"\tchain_height={info1['blocks']}\ttip={tip1}")
            if len(mined) != args.blocks:
                failures.append("generatetoaddress returned wrong block count")
            if info1["blocks"] < args.blocks:
                failures.append("chain height did not advance")
            for height in (1, 2, args.blocks):
                block = node1.cli("getblock", node1.cli("getblockhash", height))
                if "hash" not in block or "bits" not in block:
                    failures.append(f"block {height} missing validation fields")
                if block.get("bits") != "207fffff" and height <= args.blocks:
                    failures.append(f"block {height} unexpected bits {block.get('bits')}")
            print(f"block_validation\theight=1..{args.blocks}\tstatus={'PASS' if not failures else 'CHECK'}")

            node1.stop()
            node1.start()
            info_restart = node1.cli("getblockchaininfo")
            tip_restart = node1.cli("getbestblockhash")
            print(f"restart\tblocks={info_restart['blocks']}\ttip={tip_restart}")
            if tip_restart != tip1:
                failures.append("tip changed after restart")

            node2.start()
            synced = wait_for_height(node2, args.blocks)
            tip2 = node2.cli("getbestblockhash") if synced else "timeout"
            print(f"multi_node_sync\tblocks={node2.cli('getblockchaininfo')['blocks']}"
                  f"\ttip={tip2}\tstatus={'PASS' if synced and tip2 == tip1 else 'FAIL'}")
            if not synced or tip2 != tip1:
                failures.append("node2 did not sync to node1 tip")
        finally:
            node2.stop()
            node1.stop()

    print(f"node_validation_failures={len(failures)}")
    for failure in failures:
        print(f"failure\t{failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
