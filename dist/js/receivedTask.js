var taskListInfo = avalon.define({
    $id: "taskList",
    data: []
})

// solve the problem that swal do not focus
$.fn.modal.Constructor.prototype.enforceFocus = function () {};

const {ipcRenderer} = require('electron')

var taskManager = ipcRenderer.sendSync('synchronous-taskManager');

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

var submitDataModal = avalon.define({
  $id: 'submitDataModal',
  taskname:'',
  thisTaskInfo:'',
  data: []
})

$(document).on("click",".commitData",function(){
  var index = $(this).parent().index();
  var abi = JSON.parse(taskListInfo.data[index].abi);
  var temp_data = []
  for (var i = abi.length - 1; i >= 0; i--) {
    if(abi[i].name == 'commitTask'){
      for (var j = abi[i].inputs.length - 1; j >= 0; j--) {
        if(abi[i].inputs[j].name == 'dataHash') continue;
        temp_data.push({'name':abi[i].inputs[j].name,'type':abi[i].inputs[j].type});
      }
      break;
    }
  }
  submitDataModal.thisTaskInfo = taskListInfo.data[index];
  submitDataModal.taskname = taskListInfo.data[index].name;
  submitDataModal.data = temp_data;
  $("#commitDataModal").click();
})

$("#submitSensedData").on("click",function(){
  swal({
    title: 'Are you sure to submit data?',
    type: 'warning',
    showCancelButton: true,
    confirmButtonColor: '#3085d6',
    cancelButtonColor: '#d33',
    confirmButtonText: 'Yes'
  }).then((result) => {
    if (result.value) {
      var sensingData = [];
      for (var i = $(".inputs-item").length - 1; i >= 0; i--) {
        var item = $(".inputs-item").eq(i);
        var name = item.find('span:eq(0)').text();
        var type = item.find('input:eq(0)').attr('placeholder');
        var value = item.find('input:eq(0)').val();
        if(type == 'int256' && /^-?\d+$/.test(value) == false){
          swal({
            type: 'error',
            title: 'Variable value is not an integer type!'
          });
          return;
        }else if(type == 'bytes32' && (/^\s*$/.test(value) || value.length > 32)){ // value is empty or too long
          swal({
            type: 'error',
            title: 'Variable value cannot be empty or exceed 32 bytes!'
          });
          return;
        }
        sensingData.push({'name':name,'type':type,'value':value});
      }
      swal({
        title: 'Enter your password',
        input: 'password',
        showCancelButton: true,
        inputAttributes: {
          autocapitalize: 'off'
        }
      }).then((result) => {
        if(result.value != undefined || result.value != null){
          web3.personal.unlockAccount(web3.eth.defaultAccount,result.value,function(err,result){
            if(!err){
              var dataHash = ipcRenderer.sendSync('synchronous-addSensedData', JSON.stringify(sensingData));
              console.log(dataHash)
              console.log(ipcRenderer.sendSync('synchronous-catSensedData', dataHash))
              var sensing = web3.eth.contract(JSON.parse(submitDataModal.thisTaskInfo.abi));
              var sensingContract = sensing.at(submitDataModal.thisTaskInfo.contract);
              if(sensingData.length == 2){
                sensingContract.commitTask(dataHash, sensingData[0].value, sensingData[1].value, {from:web3.eth.defaultAccount, gas:1000000}, function(err,res){
                  if(!err){
                    submitSuccess(res, sensingContract);
                  }else{
                    submitError(err);
                  }
                })
              }else if(sensingData.length == 3){
                sensingContract.commitTask(dataHash, sensingData[0].value, sensingData[1].value, sensingData[2].value, {from:web3.eth.defaultAccount, gas:1000000}, function(err,res){
                  if(!err){
                    submitSuccess(res, sensingContract);
                  }else{
                    submitError(err);
                  }
                })
              }else if(sensingData.length == 4){
                sensingContract.commitTask(dataHash, sensingData[0].value, sensingData[1].value, sensingData[2].value, sensingData[3].value, {from:web3.eth.defaultAccount, gas:1000000}, function(err,res){
                  if(!err){
                    submitSuccess(res, sensingContract);
                  }else{
                    submitError(err);
                  }
                })
              }
            }else{
              swal({
                type: 'error',
                title: 'Password is not correct!'
              })
            }
          })
        }
      });
    }
  });
})

function submitSuccess(res, sensingContract){
  swal({
    type: 'success',
    title: 'You have submitted sensing data successfully!',
    text: res.toString()
  }).then((res) => {
    recoverSubmitModal();
    var addr = submitDataModal.thisTaskInfo.contract;
    sensingContract.state(function(err,res){
      if(res.toString() == '3'){
        taskManagementContract.changeState(addr, 3, {from: web3.eth.defaultAccount}, function(err,res){
          // do nothing
        });
      }
    })
  });
}

function submitError(err){
  swal({
    type: 'error',
    title: 'Oops...',
    text: err.toString()
  }).then((res) => {
    recoverSubmitModal();
  });
}

function recoverSubmitModal(){
  $("#closeSubmitData").click();
  for (var i = $(".inputs-item").length - 1; i >= 0; i--) {
    $(".inputs-item").eq(i).find('input:eq(0)').val('');
  }
}