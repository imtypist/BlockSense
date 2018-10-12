var txns = avalon.define({
    $id: "txns",
    data: [
    	{"txn":"0X4825811A5E63458953CFF8F5...","sender":"0xdd7d3348dd968f791...","receiver":"0x8a6bc6ad4c153c...","amount":0.0014,"time":24},
    	{"txn":"0XC782AD18B10DD834048A28...","sender":"0x97c2c3677d1ad1756...","receiver":"0xdd7d3348dd968f791...","amount":0.0014,"time":24},
    	{"txn":"0X76F3027209611780226742C2...","sender":"0xdd7d3348dd968f791...","receiver":"0x43a6936e007f1fe...","amount":0.0014,"time":24},
    	{"txn":"0X6676F49A43CFF46D3895A2B...","sender":"0xdd7d3348dd968f791...","receiver":"0x6dfe212d1461014...","amount":0,"time":24},
    	{"txn":"0XC778CDFBB792B996AB1F716...","sender":"0xe54fcce2ada533c...","receiver":"0xdd7d3348dd968f791...","amount":0,"time":24}
    ]
})

if (typeof web3 !== 'undefined') {
    web3 = new Web3(web3.currentProvider);
} else {
    // set the provider you want from Web3.providers
    web3 = new Web3(new Web3.providers.HttpProvider("http://localhost:8545"));
    web3.eth.defaultAccount = localStorage.getItem("defaultAccount");
}