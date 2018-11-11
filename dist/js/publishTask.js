var taskList = avalon.define({
    $id: "taskList",
    data: [
    	{"status":1,"contract":"0X4825811A5E63458953CFF8F5...","name":"感知任务4","description":"收集闵行校区内SSID为SJTU的WiFi信号强度"},
    	{"status":0,"contract":"0X4825811A5E63458953CFF8F5...","name":"感知任务6","description":"收集闵行校区内SSID为SJTU的WiFi信号强度"}
    ]
})

$("#add_contract_var").on("click",function(){
	$(this).before($("#contract_var").prop("outerHTML"));
})

const {ipcRenderer} = require('electron')

var taskManager = ipcRenderer.sendSync('synchronous-taskManager');
// console.log(taskManager)

if (typeof web3 !== 'undefined') {
    web3 = new Web3(web3.currentProvider);
} else {
    // set the provider you want from Web3.providers
    web3 = new Web3(new Web3.providers.HttpProvider("http://localhost:8545"));
    web3.eth.defaultAccount = localStorage.getItem("defaultAccount");
}

var taskManagement = web3.eth.contract(JSON.parse(taskManager.abi));
var taskManagementContract = taskManagement.at(taskManager.address);

var taskList = taskManagementContract.taskList({}, {fromBlock: 0, toBlock: 'latest'});

taskList.watch(function(error, result){
  if(!error){
  	console.log(result);
  }else{
  	console.log(error);
  }
});

// taskManagementContract.addTask('0x5b84650648B1DFc1af387FFe9cd2c33385e28Db7','testabi','testtask','testtesttesttest',{from:web3.eth.defaultAccount},function(err,res){console.log(res)})