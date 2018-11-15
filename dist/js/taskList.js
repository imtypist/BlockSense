var taskListInfo = avalon.define({
    $id: "taskList",
    data: []
})
//state => 1:ACTIVE 2:ABORT 3:COMPLETE

if (typeof web3 !== 'undefined') {
    web3 = new Web3(web3.currentProvider);
} else {
    // set the provider you want from Web3.providers
    web3 = new Web3(new Web3.providers.HttpProvider("http://localhost:8545"));
    web3.eth.defaultAccount = localStorage.getItem("defaultAccount");
}

const {ipcRenderer} = require('electron')

var taskManager = ipcRenderer.sendSync('synchronous-taskManager');
// console.log(taskManager)

var taskManagement = web3.eth.contract(JSON.parse(taskManager.abi));
var taskManagementContract = taskManagement.at(taskManager.address);

var taskList = taskManagementContract.taskList({}, {fromBlock: 0, toBlock: 'latest'});

taskList.watch(function(error, result){
  if(!error){
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
    var temp_task = taskListInfo.data[index];
    var sensing = web3.eth.contract(JSON.parse(temp_task.abi));
    var sensingContract = sensing.at(temp_task.contract);
    sensingContract.rewardUnit(function(err,res){
        temp_task['rewardUnit'] = res.toString();
        sensingContract.rewardNum(function(err,res){
            temp_task['rewardNum'] = res.toString();
            sensingContract.dataCount(function(err,res){
                temp_task['dataCount'] = res.toString();
                singleTask.data = temp_task;
                $("#clickToAppear").click();
            })
        })
    })
})

$(document).on("click","#acceptTask",function(){
    if(singleTask.data.status == 2){
        swal({
          type: 'error',
          title: 'This task has been aborted!'
        })
        return;
    }
    if(singleTask.data.status == 3){
        swal({
          type: 'error',
          title: 'This task has been completed!'
        })
        return;
    }
    if(localStorage.getItem('receivedTask') == undefined){
        localStorage.setItem('receivedTask',JSON.stringify([]));
    }
    var currentTask = JSON.parse(localStorage.getItem('receivedTask'));
    if($.inArray(singleTask.data.contract, currentTask) == -1){
        currentTask.push(singleTask.data.contract);
        localStorage.setItem('receivedTask',JSON.stringify(currentTask));
        swal({
          position: 'top-end',
          type: 'success',
          title: 'You have accepted this task successfully',
          showConfirmButton: false,
          timer: 2000
        })
    }else{
        swal({
          type: 'error',
          title: 'You have already accepted it before!'
        })
    }
})