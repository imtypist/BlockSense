from web3.auto import w3
import time
from web3 import Web3

w3.eth.defaultAccount = w3.eth.accounts[0]


sensing = w3.eth.contract(
    address=Web3.toChecksumAddress('0x37dc85ae239ec39556ae7cc35a129698152afe3c'),
    abi="[{\"constant\":false,\"inputs\":[{\"name\":\"sensingData\",\"type\":\"string\"}],\"name\":\"commitTask\",\"outputs\":[],\"payable\":false,\"stateMutability\":\"nonpayable\",\"type\":\"function\"},{\"anonymous\":false,\"inputs\":[{\"indexed\":false,\"name\":\"s\",\"type\":\"string\"}],\"name\":\"DataCommited\",\"type\":\"event\"}]",
)

st = time.time()
a = sensing.events.DataCommited.createFilter(fromBlock=3800)
print(len(a.get_all_entries()))
print(time.time() - st)
