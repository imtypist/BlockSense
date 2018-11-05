if(localStorage.getItem("defaultAccount") != null){
    window.location.href = './account.html';
}

var blocks = avalon.define({
    $id: "blocks",
    data: []
})

var txns = avalon.define({
    $id: "txns",
    data: []
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
        web3.eth.sendTransaction({from:web3.eth.accounts[1],to:web3.eth.defaultAccount,value:10000000000000000000});
        swal({
          position: 'top-end',
          type: 'success',
          title: 'Your account has been created',
          showConfirmButton: false,
          timer: 2000
        }).then((result) => {
          window.location.href = './account.html';
        })
    }else{
        swal({
          type: 'error',
          title: 'Inconsistent passwords entered!'
        })
    }
})

function syncBlocks(new_block){
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

function syncTXs(new_block){
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
                "amount": web3.fromWei(tx_info.value).toString(10),
                "time": parseInt((Date.now() / 1000 - block_info.timestamp) / 60)
            });
            num = num + 1;
            if(num >= 5) break;
        }
        i = i + 1;
    }
    txns.data = temp_data;
}

latestBlock = null
setInterval(function(){
    new_block = web3.eth.blockNumber;
    if(latestBlock != new_block){
        latestBlock = new_block;
        syncBlocks(latestBlock);
        syncTXs(latestBlock);
    }
},1000);