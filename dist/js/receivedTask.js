var taskListInfo = avalon.define({
    $id: "taskList",
    data: []
})

const {ipcRenderer} = require('electron')

var taskManager = ipcRenderer.sendSync('synchronous-taskManager');

$("#submitSensedData").on("click",function(){
	var dataHash = ipcRenderer.sendSync('synchronous-addSensedData', 'hello world');
	console.log(dataHash)
	console.log(ipcRenderer.sendSync('synchronous-catSensedData', dataHash))
})

if (typeof web3 !== 'undefined') {
    web3 = new Web3(web3.currentProvider);
} else {
    // set the provider you want from Web3.providers
    web3 = new Web3(new Web3.providers.HttpProvider("http://localhost:8545"));
    web3.eth.defaultAccount = localStorage.getItem("defaultAccount");
}

var receivedTask = JSON.parse(localStorage.getItem('receivedTask'));

var taskManagement = web3.eth.contract(JSON.parse(taskManager.abi));
var taskManagementContract = taskManagement.at(taskManager.address);

var taskList = taskManagementContract.taskList({}, {fromBlock: 0, toBlock: 'latest'});

taskList.watch(function(error, result){
  if(!error){
  	if($.inArray(result.args.contra, receivedTask) == -1) return;
    var task_info = {"status":0,"contract":result.args.contra,"name":result.args.taskName,"description":result.args.taskDescrip,"requester":0,"abi":result.args.taskabi};
    taskManagementContract.states(task_info.contract,function(err,res){
      if(!err && res[1].toString() != '0' && res[0].toString() != '0x0000000000000000000000000000000000000000'){
        task_info.status = parseInt(res[1].toString());
        task_info.requester = res[0].toString();
        taskListInfo.data.push(task_info);
      }else{
        console.log(err);
      }
    })
  }else{
    console.log(error);
  }
});

var stateChanged = taskManagementContract.stateChanged({}, {fromBlock: 'latest'});

stateChanged.watch(function(error, result){
  if(!error){
  	if($.inArray(result.args.contra, receivedTask) == -1) return;
    for(var i=0;i<taskListInfo.data.length;i++){
        if(taskListInfo.data[i].contract == result.args.contra){
            taskManagementContract.states(taskListInfo.data[i].contract,function(err,res){
              if(!err && res[1].toString() != '0' && res[0].toString() != '0x0000000000000000000000000000000000000000'){
                taskListInfo.data[i].status = parseInt(res[1].toString());
              }else{
                console.log(err);
              }
            });
            break;
        }
    }
  }else{
    console.log(error);
  }
});

var singleTask = avalon.define({
    $id: "singleTask",
    data: {}
})

$(document).on("click",".taskDetailInfo",function(){
    var index = $(this).parent().index();
    singleTask.data = taskListInfo.data[index];
    $("#clickToAppear").click();
})

$(document).on("click","#deleteTask",function(){
	swal({
      title: 'Are you sure to delete this task?',
      type: 'warning',
      showCancelButton: true,
      confirmButtonColor: '#3085d6',
      cancelButtonColor: '#d33',
      confirmButtonText: 'Yes'
    }).then((result) => {
      if (result.value) {
      	var currentTask = JSON.parse(localStorage.getItem('receivedTask'));
      	for (var i = currentTask.length - 1; i >= 0; i--) {
      		if(currentTask[i] == singleTask.data.contract){
      			currentTask.splice(i,1);
      			receivedTask = currentTask;
      			localStorage.setItem('receivedTask',JSON.stringify(currentTask));
      			break;
      		}
      	}
      	for (var i = taskListInfo.data.length - 1; i >= 0; i--) {
      		if(taskListInfo.data[i].contract == singleTask.data.contract){
      			taskListInfo.data.splice(i,1);
      			break;
      		}
      	}
      	$("#closeTaskModal").click();
      	singleTask.data = {};
      }
  	});
})