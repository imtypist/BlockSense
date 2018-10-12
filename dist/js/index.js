if(localStorage.getItem("defaultAccount") != null){
    window.location.href = './account.html';
}

var blocks = avalon.define({
    $id: "blocks",
    data: [
    	{"order":6192036,"time":24,"miner":"Ethermine","txns":162,"size":28,"hash":3.07068},
    	{"order":6192035,"time":52,"miner":"0xd4383232c8d1dbe0...","txns":69,"size":10,"hash":3.04352},
    	{"order":6192034,"time":62,"miner":"MiningPoolHub_1","txns":125,"size":11,"hash":3.03439},
    	{"order":6192033,"time":73,"miner":"Ethermine","txns":141,"size":20,"hash":3.28133},
    	{"order":6192032,"time":93,"miner":"0x35f61dfb08ada13...","txns":126,"size":6,"hash":3.04618}
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

if (typeof web3 !== 'undefined') {
    web3 = new Web3(web3.currentProvider);
} else {
    // set the provider you want from Web3.providers
    web3 = new Web3(new Web3.providers.HttpProvider("http://localhost:8545"));
}

$("#signup").on("click",function(){
    var passwd = $("#inputPassword").val();
    if(passwd == $("#repeatPassword").val() && passwd != ''){
        web3.eth.defaultAccount = web3.personal.newAccount(passwd);
        localStorage.setItem("defaultAccount", web3.eth.defaultAccount);
        window.location.href = './account.html';
    }else{
        alert('两次密码输入不相同或密码为空');
    }
})

function syncBlocks(){
    new_block = web3.eth.blockNumber;
    i = 0;
    temp_data = [];
    while(i < 5 && (new_block - i) >= 0){
        block_info = web3.eth.getBlock(new_block - i);
        temp_data.push({
            "order": block_info.number,
            "time": block_info.timestamp,
            "miner": block_info.miner,
            "txns": block_info.transactions.length,
            "size": block_info.size,
            "hash": block_info.hash
        });
        i = i + 1;
    }
    blocks.data = temp_data;
}

function syncTXs(){
    new_block = web3.eth.blockNumber;
    i = 0;
    num = 0;
    temp_data = [];
    while(i < 5 && (new_block - i) >= 0 && num < 5){
        block_info = web3.eth.getBlock(new_block - i);
        for(j = 0; j < block_info.transactions.length; j++){
            tx_info = web3.eth.getTransaction(block_info.transactions[j]);
            temp_data.push({
                "txn": tx_info.hash,
                "sender": tx_info.from,
                "receiver": tx_info.to,
                "amount": tx_info.value.toString(10),
                "time": parseInt((Date.now() / 1000 - block_info.timestamp) / 60)
            });
            num = num + 1;
            if(num >= 5) break;
        }
    }
    txns.data = temp_data;
}

setInterval(function(){
    syncBlocks();
    syncTXs();
},1000);