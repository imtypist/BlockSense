from web3.auto import w3
import sys
from web3 import Web3
import json
from solc import compile_source
from web3.contract import ConciseContract

path_to_contract = sys.argv[1]
contract_name = sys.argv[2]
with open(path_to_contract, 'r') as f:
	contract_source_code = f.read()


compiled_sol = compile_source(contract_source_code)
contract_interface = compiled_sol['<stdin>:' + contract_name]

w3.eth.defaultAccount = w3.eth.accounts[0]

contract_instance = w3.eth.contract(abi=contract_interface['abi'], bytecode=contract_interface['bin'])

tx_hash = contract_instance.constructor().transact()
tx_receipt = w3.eth.waitForTransactionReceipt(tx_hash)

with open('./deployed/' + contract_name + '.info', 'w+') as f:
	f.write(str(tx_receipt.contractAddress) + '\n')
	f.write(json.dumps(contract_interface['abi']))