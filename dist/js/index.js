var blocks = avalon.define({
    $id: "blocks",
    data: [
    	{"order":6192036,"time":24,"miner":"Ethermine","txns":162,"speed":28,"reward":3.07068},
    	{"order":6192035,"time":52,"miner":"0xd4383232c8d1dbe0...","txns":69,"speed":10,"reward":3.04352},
    	{"order":6192034,"time":62,"miner":"MiningPoolHub_1","txns":125,"speed":11,"reward":3.03439},
    	{"order":6192033,"time":73,"miner":"Ethermine","txns":141,"speed":20,"reward":3.28133},
    	{"order":6192032,"time":93,"miner":"0x35f61dfb08ada13...","txns":126,"speed":6,"reward":3.04618}
    ]
})

var txns = avalon.define({
    $id: "txns",
    data: [
    	{"txn":"0X4825811A5E63458953CFF8F5...","sender":"0xdd7d3348dd968f791...","receiver":"0x8a6bc6ad4c153c...","amount":0.0014,"time":24},
    	{"txn":"0XC782AD18B10DD834048A28...","sender":"0x97c2c3677d1ad1756...","receiver":"0x8e87e2656c4260...","amount":0.0014,"time":24},
    	{"txn":"0X76F3027209611780226742C2...","sender":"0x9d482bd79bd3a96...","receiver":"0x43a6936e007f1fe...","amount":0.0014,"time":24},
    	{"txn":"0X6676F49A43CFF46D3895A2B...","sender":"0xe480e1696f5c4df...","receiver":"0x6dfe212d1461014...","amount":0,"time":24},
    	{"txn":"0XC778CDFBB792B996AB1F716...","sender":"0xe54fcce2ada533c...","receiver":"0x6dfe212d1461014...","amount":0,"time":24}
    ]
})

$("#signup").on("click",function(){
	window.location.href = './account.html';
})