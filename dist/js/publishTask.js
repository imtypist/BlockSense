var taskListInfo = avalon.define({
    $id: "taskList",
    data: []
})

$(document).on("click","#add_contract_var",function(){
	$(this).before($(".contract_var:eq(0)").prop("outerHTML"));
})

$(document).on("click",".type_li",function(){
	$(this).parent().parent().parent().find("button:eq(0)").text($(this).text());
})

$(document).on("click",".delete_var",function(){
	if($(".delete_var").length <= 1){
		swal({
          type: 'info',
          title: 'You can leave it blank if you do not need additional variables.'
        })
	}else{
		$(this).parent().remove();
	}
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
    var task_info = {"status":0,"contract":result.args.contra,"name":result.args.taskName,"description":result.args.taskDescrip,"requester":0,"abi":result.args.taskabi};
    taskManagementContract.states(task_info.contract,function(err,res){
      if(!err && res[1].toString() != '0' && res[0].toString() == web3.eth.defaultAccount){ // filter
        task_info.status = parseInt(res[1].toString());
        task_info.requester = res[0].toString();
        taskListInfo.data.push(task_info);
      }else{
        if(err) console.log(err);
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
              if(!err && res[1].toString() != '0' && res[0].toString() == web3.eth.defaultAccount){ // filter
                taskListInfo.data[i].status = parseInt(res[1].toString());
              }else{
                if(err) console.log(err);
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

$(document).on("click",".terminateTask",function(){
	var that = $(this);
	swal({
      title: 'Are you sure to abort this task?',
      type: 'warning',
      showCancelButton: true,
      confirmButtonColor: '#3085d6',
      cancelButtonColor: '#d33',
      confirmButtonText: 'Yes'
    }).then((result) => {
      if (result.value) {
      	swal({
          title: 'Enter your password',
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
                	var index = that.parent().index();
			      	taskManagementContract.changeState(taskListInfo.data[index].contract, 2, {from: web3.eth.defaultAccount}, function(err,res){
			      		if(!err){
			      			swal({
					          position: 'top-end',
					          type: 'success',
					          title: 'You have aborted this task successfully',
					          showConfirmButton: false,
					          timer: 2000
					        })
			      		}
			      	});
                }else{
                    swal({
                      type: 'error',
                      title: 'Password is not correct!'
                    })
                }
            })
        });
      }
  	});
})

// solve the problem that swal do not focus
$.fn.modal.Constructor.prototype.enforceFocus = function () {};

$(document).on("click","#publishTask",function(){
	$(this).attr('disabled','disabled');
	var that = $(this);
	var name = $("#name").val();
	if(!name || name.trim().length == 0){
		swal({
          type: 'error',
          title: 'Contract name cannot be empty!'
        });
        $(this).removeAttr('disabled');
        return;
	}
	var description = $("#description").val();
	if(!description || description.trim().length == 0){
		swal({
          type: 'error',
          title: 'Task description cannot be empty!'
        });
        $(this).removeAttr('disabled');
        return;
	}
	var eachBonus = parseInt($("#eachBonus").val());
	var dataNumber = parseInt($("#dataNumber").val());
	if(!eachBonus || eachBonus <= 0 || eachBonus == NaN || !dataNumber || dataNumber <= 0 || dataNumber == NaN){
		swal({
          type: 'error',
          title: 'Please enter a positive integer!'
        });
        $(this).removeAttr('disabled');
        return;
	}
	var condition = $("#condition").val();
	var reg = /^(>|<|>=|<=|==|!=)\s*-?\d+$/;
	if(!condition || !reg.test(condition.trim())){
		swal({
          type: 'error',
          title: 'You do not write in a correct boolean pattern! (condition symbol + integer)'
        });
        $(this).removeAttr('disabled');
        return;
	}
	var compileData = {'condition':condition.trim()};
	// additional variables
	for (var i = $(".contract_var").length - 1; i >= 0; i--) {
		var el = $(".contract_var").eq(i);
		var type = el.find('button.var_type:eq(0)').text();
		var property = el.find('button.var_property:eq(0)').text();
		var var_name = el.find('input.var_name:eq(0)').val();
		var var_value = el.find('input.var_value:eq(0)').val();
		if(/^\s*$/.test(var_name) && /^\s*$/.test(var_value) && $(".contract_var").length == 1){ // no addtitional vars
			break;
		}else if(/^\s*$/.test(var_name) && /^\s*$/.test(var_value) && $(".contract_var").length > 1){ // extra blank
			swal({
	          type: 'error',
	          title: 'Please delete extra blank items before publishing task!'
	        });
	        $(this).removeAttr('disabled');
	        return;
		}else if(/^[a-zA-Z]+\w*$/.test(var_name) == false){ // variable name in wrong form
			swal({
	          type: 'error',
	          title: 'Variable name is not in correct form!'
	        });
	        $(this).removeAttr('disabled');
	        return;
		}else if(compileData[var_name]){ // the same variable name exist
			swal({
	          type: 'error',
	          title: 'Variable names cannot be the same!'
	        });
	        $(this).removeAttr('disabled');
	        return;
		}else if(type == 'int' && /^-?\d+$/.test(var_value) == false){ // value is not int
			swal({
	          type: 'error',
	          title: 'Variable value is not an integer type!'
	        });
	        $(this).removeAttr('disabled');
	        return;
		}else if(type == 'bytes32' && (/^\s*$/.test(var_value) || var_value.length > 32)){ // value is empty or too long
			swal({
	          type: 'error',
	          title: 'Variable value cannot be empty or exceed 32 bytes!'
	        });
	        $(this).removeAttr('disabled');
	        return;
		}else{ // is a legal variable
			compileData[var_name] = {'value': var_value, 'type': type, 'property': property};
		}
	}
	swal({
      title: 'Are you sure to publish this task?',
      type: 'warning',
      showCancelButton: true,
      confirmButtonColor: '#3085d6',
      cancelButtonColor: '#d33',
      confirmButtonText: 'Yes'
    }).then((result) => {
      if (result.value) {
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
	                	var cc = $(".progress:eq(0)").find("strong:eq(0)"); // compile contract text
						var dc = $(".progress:eq(0)").find("strong:eq(1)"); // depoly contract text
						$(".progress:eq(0)").show();
						var cc_progress = cc.parent().parent();
						var dc_progress = dc.parent().parent();
						cc_progress.css('width','25%');
						var cc_icon = cc_progress.find("span.glyphicon:eq(0)");
						var dc_icon = dc_progress.find("span.glyphicon:eq(0)");
						setTimeout(function(){ // meanless sleep
							var contract_info = ipcRenderer.sendSync('synchronous-compileTask',JSON.stringify(compileData));
							if(contract_info == -1){ // compile failed
								cc_progress.removeClass("progress-bar-success").addClass("progress-bar-danger");
								cc_progress.css('width','50%');
								cc_icon.removeClass("glyphicon-cog").addClass("glyphicon-remove");
								cc.text("合约编译失败");
						        swal({
			                      type: 'error',
			                      title: 'Compile contract failed!'
			                    }).then((result) => {
			                    	recoverModalAfterPublishTask(cc, dc, cc_progress, dc_progress, cc_icon, dc_icon, 1, 1);
			                    	that.removeAttr('disabled');
			                    });
							}else{
								cc_progress.css('width','50%');
								cc_icon.removeClass("glyphicon-cog").addClass("glyphicon-ok");
								cc.text("合约编译成功");
								setTimeout(function(){
									dc_progress.css('width','25%');
								},800);
			                	setTimeout(function(){ // meanless sleep again
									contract_info = JSON.parse(contract_info);
									let bytecode = contract_info.bytecode;
								  	let abi = JSON.parse(contract_info.abi);
								  	let gasEstimate = web3.eth.estimateGas({data: bytecode});
								  	console.log(gasEstimate)
									let MyContract = web3.eth.contract(abi);
									MyContract.new(eachBonus, dataNumber, description.trim(), name.trim(), {from:web3.eth.defaultAccount, data:bytecode, gas:gasEstimate*2, value:eachBonus*dataNumber+100000000}, function(err, myContract){
									   if(!err) {
									      if(!myContract.address) {
									          console.log(myContract.transactionHash) // The hash of the transaction, which deploys the contract
									      }else{
									          console.log(myContract.address) // the contract address
									          taskManagementContract.addTask(myContract.address,contract_info.abi,name.trim(),description.trim(),{from:web3.eth.defaultAccount, gas:gasEstimate},function(err,res){
									        	if(!err){
													dc_progress.css('width','50%');
													dc_icon.removeClass("glyphicon-cog").addClass("glyphicon-ok");
													dc.text("合约部署成功");
													swal({
									                  type: 'success',
									                  title: 'Depoly contract successfully!'
									                }).then((result) => {
									                  recoverModalAfterPublishTask(cc, dc, cc_progress, dc_progress, cc_icon, dc_icon, 0, 0);
									                  that.removeAttr('disabled');
									                });
									        	}else{
									        		console.log(err);
									        		dc_progress.removeClass("progress-bar-info").addClass("progress-bar-danger");
													dc_progress.css('width','50%');
													dc_icon.removeClass("glyphicon-cog").addClass("glyphicon-remove");
													dc.text("合约部署失败");
												    swal({
								                      type: 'error',
								                      title: 'Depoly contract failed!'
								                    }).then((result) => {
								                    	recoverModalAfterPublishTask(cc, dc, cc_progress, dc_progress, cc_icon, dc_icon, 0, 1);
								                    	that.removeAttr('disabled');
								                    });
									        	}
										      });
										  }
									   }else{
									   	  dc_progress.removeClass("progress-bar-info").addClass("progress-bar-danger");
										  dc_progress.css('width','50%');
										  dc_icon.removeClass("glyphicon-cog").addClass("glyphicon-remove");
										  dc.text("合约部署失败");
									      swal({
						                    type: 'error',
						                    title: 'Depoly contract failed!'
						                  }).then((result) => {
						                    recoverModalAfterPublishTask(cc, dc, cc_progress, dc_progress, cc_icon, dc_icon, 0, 1);
						                    that.removeAttr('disabled');
						                  });
									   }
									 });
								},1500);
							}
						},1500);
	                }else{
	                    swal({
	                      type: 'error',
	                      title: 'Password is not correct!'
	                    })
	                    that.removeAttr('disabled');
	                }
	            })
        	}
        })
      }
    });
});

function recoverModalAfterPublishTask(cc, dc, cc_progress, dc_progress, cc_icon, dc_icon, cc_status, dc_status){ // success: 0, failure: 1
	$("#closeModal").click();
	// empty inputs
	$("#name").val('');
	$("#description").val('');
	$("#eachBonus").val('');
	$("#dataNumber").val('');
	$("#condition").val('');
	for (var i = $(".contract_var").length - 1; i >= 1; i--) {
		$(".contract_var").eq(i).remove();
	}
	var el = $(".contract_var").eq(0);
	el.find('button.var_type:eq(0)').text('int');
	el.find('button.var_property:eq(0)').text('public');
	el.find('input.var_name:eq(0)').val('');
	el.find('input.var_value:eq(0)').val('');
	// recover progress
	$(".progress:eq(0)").hide();
	cc.text('正在编译合约');
	cc_progress.css('width','0%');
	if(cc_status == 1){
		cc_progress.removeClass("progress-bar-danger").addClass("progress-bar-success");
		cc_icon.removeClass("glyphicon-remove").addClass("glyphicon-cog");
	}else if(cc_status == 0){
		cc_icon.removeClass("glyphicon-ok").addClass("glyphicon-cog");
		dc.text('正在部署合约');
		dc_progress.css('width','0%');
		if(dc_status == 1){
			dc_progress.removeClass("progress-bar-danger").addClass("progress-bar-info");
			dc_icon.removeClass("glyphicon-remove").addClass("glyphicon-cog");
		}else if(dc_status == 0){
			dc_icon.removeClass("glyphicon-ok").addClass("glyphicon-cog");
		}
	}
	return 0;
}

$(document).on("click",'.getSensingData',function(){
	$('#sensingDataModal').click();
})