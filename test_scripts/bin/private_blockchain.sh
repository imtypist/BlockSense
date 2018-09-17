#!/bin/bash

geth=${GETH:-geth}

$geth --datadir data --networkid 31415926 --rpc --rpccorsdomain "*" --rpcapi db,eth,net,web3,personal,admin,miner,txpool --nodiscover console --unlock 0 --password passwd
