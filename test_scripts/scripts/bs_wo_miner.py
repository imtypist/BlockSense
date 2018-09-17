from web3.auto import w3
import time
from web3 import Web3
import random
import threading
import os

w3.eth.defaultAccount = w3.eth.accounts[0]

path = '../results/'

with open('../deployed/CrowdSense.info','r') as f:
	contract_info = f.readlines()

sensing = w3.eth.contract(
    address=Web3.toChecksumAddress(contract_info[0].strip()),
    abi=contract_info[1].strip(),
)

def sendTx(key, value, degree):
	global latency
	tx_hash = sensing.functions.commitTask(key, value, degree).transact()
	tx_delay = time.time()
	receipt = w3.eth.waitForTransactionReceipt(tx_hash)
	latency = latency + (time.time() - tx_delay)


requests = [32,] #[4, 8, 12, 16, 20, 24, 28, 32]
size = '8'
hash_str = 'QmW2WQi7j6c7UgJTarActp7tDNikE4B2qXtFCfLPdsgaTQ'

with open('../datasets/'+size+'KB' ,'r') as f:
	fp = open(path + 'bs_wo_miner_data_'+size+'kb_request_from_4_to_32','a+')
	for el in requests:
		for i in range(64):
			print(i+1)
			latency = 0
			threads = []
			st = time.time()
			for i in range(el):
				t = threading.Thread(target=sendTx, args=(hash_str + str(random.random()),f.read(),-50))
				t.start()
				threads.append(t)
			request_time = time.time() - st
			for t in threads:
				t.join()
			tx_time = time.time() - st
			a = [str(el), str(request_time), str(tx_time), str(latency), '\n']
			fp.write(' '.join(a))

fp.close()