var txns = avalon.define({
    $id: "txns",
    data: []
})

var account_info = avalon.define({
    $id: "account_info",
    data: {}
})

if (typeof web3 !== 'undefined') {
    web3 = new Web3(web3.currentProvider);
} else {
    // set the provider you want from Web3.providers
    web3 = new Web3(new Web3.providers.HttpProvider("http://localhost:8545"));
    web3.eth.defaultAccount = localStorage.getItem("defaultAccount");
}

latestBlock = null;

function initialInfo(){
    latestBlock = web3.eth.blockNumber;
    temp_data = {
        'nickname':'imtypist',
        'address': web3.eth.defaultAccount,
        'value': web3.fromWei(web3.eth.getBalance(web3.eth.defaultAccount)).toString(),
        'blockNumber': latestBlock,
        'isMining': web3.eth.mining,
    }
    account_info.data = temp_data;
}

initialInfo();

$("#transfer").on("click",function(){
    swal({
      title: 'Are you sure to transfer?',
      type: 'warning',
      showCancelButton: true,
      confirmButtonColor: '#3085d6',
      cancelButtonColor: '#d33',
      confirmButtonText: 'Yes'
    }).then((result) => {
      if (result.value) {
        swal({
          title: 'Confirm your password again',
          input: 'password',
          inputAttributes: {
            autocapitalize: 'off'
          },
          showCancelButton: true,
          confirmButtonText: 'Confirm',
          showLoaderOnConfirm: true,
          allowOutsideClick: () => !swal.isLoading()
        }).then((result) => {
            web3.personal.unlockAccount(web3.eth.defaultAccount,result.value,function(err,result){
                if(!err){
                    var amount = $("#transfer_amount").val();
                    var to = $("#to_address").val();
                    if(web3.isAddress(to) && amount > 0){
                        web3.eth.sendTransaction({from:web3.eth.defaultAccount,to:to,value:amount},function(err,result){
                            if(!err){
                                swal({
                                  title: 'Transfer Success',
                                  text: 'transaction hash: ' + result,
                                  width: 600,
                                  padding: '3em',
                                  background: '#fff url(dist/img/trees.png)',
                                  backdrop: `
                                    rgba(0,0,123,0.4)
                                    url("dist/img/nyan-cat.gif")
                                    center left
                                    no-repeat
                                  `
                                });
                                account_info.data.value = web3.fromWei(web3.eth.getBalance(web3.eth.defaultAccount)).toString();
                            }else{
                                swal({
                                  type: 'error',
                                  title: 'Oops...',
                                  text: err.toString()
                                })
                            }
                        })
                    }else{
                        swal({
                          type: 'error',
                          title: 'Address is not correct or amount is not a positive value!'
                        })
                    }
                }else{
                    swal({
                      type: 'error',
                      title: 'Password is not correct!'
                    })
                }
            })
        })
      }
    })
})

function syncTXs(new_block){
    i = 0;
    num = 0;
    temp_data = [];
    while(i < 5 && (new_block - i) >= 0 && num < 5){
        block_info = web3.eth.getBlock(new_block - i);
        for(j = 0; j < block_info.transactions.length; j++){
            tx_info = web3.eth.getTransaction(block_info.transactions[j]);
            if(!(tx_info.from == web3.eth.defaultAccount || tx_info.to == web3.eth.defaultAccount)) continue;
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
        account_info.data.blockNumber = latestBlock;
        syncTXs(latestBlock);
    }
},1000);

$("#logout").on("click",function(){
    localStorage.clear();
    window.location.href = './index.html';
});