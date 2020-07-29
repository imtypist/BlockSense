import hashlib
import json
from time import time
from urllib.parse import urlparse
from uuid import uuid4


class Blockchain:
    def __init__(self):
        self.current_transactions = []
        self.chain = []
        self.nodes = set()
        # Generation interval of blocks (seconds)
        self.block_interval = 14
        # Minimum gas needed
        self.min_gas = 21000
        # Update every 5 blocks
        self.current_local_avg_gas = 1000
        # Generate a globally unique address for this node
        self.address = str(uuid4()).replace('-', '')
        # Create the genesis block
        self.new_block(previous_hash='0', local_avg_gas=0, self.address)

    def register_node(self, instance):
        """
        Add a new node to the list of nodes

        :instance: instance object of node.'
        """

        self.nodes.add(instance)

    def valid_block(self, block):
        """
        Determine if a given block is valid

        :param block: A newest block
        :return: True if valid, False if not
        """

        last_block = self.chain[-1]

        print(f'{last_block}')
        print(f'{block}')
        print("\n-----------\n")
        # Check that the index of the block is larger than local chain
        if last_block['index'] >= block['index']:
            return False

        # Check that the hash of the block is correct
        last_block_hash = self.hash(last_block)
        if block['previous_hash'] != last_block_hash:
            return False

        # Check that the Proof of Data is correct
        if not self.valid_proof(last_block['proof'], block['proof'], last_block_hash):
            return False

        last_block = block
        current_index += 1

        return True
    
    @staticmethod
    def valid_proof(last_proof, proof, last_hash):
        """
        Validates the Proof

        :param last_proof: <int> Previous Proof
        :param proof: <int> Current Proof
        :param last_hash: <str> The hash of the Previous Block
        :return: <bool> True if correct, False if not.

        """

        guess = f'{last_proof}{proof}{last_hash}'.encode()
        guess_hash = hashlib.sha256(guess).hexdigest()
        return guess_hash[:4] == "0000"

    def resolve_conflicts(self):
        """
        This is our consensus algorithm, it resolves conflicts
        by replacing our chain with the longest one in the network.

        :return: True if our chain was replaced, False if not
        """

        neighbours = self.nodes
        new_chain = None

        # We're only looking for chains longer than ours
        max_length = len(self.chain)

        # Grab and verify the chains from all the nodes in our network
        for node in neighbours:
            response = requests.get(f'http://{node}/chain')

            if response.status_code == 200:
                length = response.json()['length']
                chain = response.json()['chain']

                # Check if the length is longer and the chain is valid
                if length > max_length and self.valid_chain(chain):
                    max_length = length
                    new_chain = chain

        # Replace our chain if we discovered a new, valid chain longer than ours
        if new_chain:
            self.chain = new_chain
            return True

        return False

    def new_block(self, local_avg_gas, previous_hash, address):
        """
        Create a new Block in the Blockchain

        :param avg_proof_gas: The average proof gas given by the Proof of Data algorithm
        :param previous_hash: Hash of previous Block
        :param address: Identifier of miner
        :return: New Block
        """

        block = {
            'index': len(self.chain) + 1,
            'timestamp': time(),
            'transactions': self.current_transactions,
            'local_avg_gas': local_avg_gas,
            'previous_hash': previous_hash or self.hash(self.chain[-1]),
            'address': address
        }

        # Reset the current list of transactions
        self.current_transactions = []

        self.chain.append(block)
        return block

    def new_transaction(self, sender, recipient, amount):
        """
        Creates a new transaction to go into the next mined Block

        :param sender: Address of the Sender
        :param recipient: Address of the Recipient
        :param amount: Amount
        :return: The index of the Block that will hold this transaction
        """
        self.current_transactions.append({
            'sender': sender,
            'recipient': recipient,
            'amount': amount,
        })

        return self.last_block['index'] + 1

    @property
    def last_block(self):
        return self.chain[-1]

    @staticmethod
    def hash(block):
        """
        Creates a SHA-256 hash of a Block

        :param block: Block
        """

        # We must make sure that the Dictionary is Ordered, or we'll have inconsistent hashes
        block_string = json.dumps(block, sort_keys=True).encode()
        return hashlib.sha256(block_string).hexdigest()

    def proof_of_data(self, last_block):
        """
        Simple Proof of Work Algorithm:

         - Find a number p' such that hash(pp') contains leading 4 zeroes
         - Where p is the previous proof, and p' is the new proof
         
        :param last_block: <dict> last Block
        :return: <int>
        """

        last_proof = last_block['proof']
        last_hash = self.hash(last_block)

        proof = 0
        while self.valid_proof(last_proof, proof, last_hash) is False:
            proof += 1

        return proof


if __name__ == '__main__':
    # Instantiate the Blockchain
    blockchain = Blockchain()