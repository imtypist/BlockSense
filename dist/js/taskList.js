var taskList = avalon.define({
    $id: "taskList",
    data: [
    	{"status":0,"contract":"0x51a20e7b04b6aab7f8f7ff3f...","name":"感知任务1","description":"收集闵行校区内SSID为SJTU的WiFi信号强度"},
    	{"status":2,"contract":"0X4825811A5E63458953CFF8F5...","name":"感知任务2","description":"收集闵行校区内SSID为SJTU的WiFi信号强度"},
    	{"status":0,"contract":"0X4825811A5E63458953CFF8F5...","name":"感知任务3","description":"收集闵行校区内SSID为SJTU的WiFi信号强度"},
    	{"status":1,"contract":"0X4825811A5E63458953CFF8F5...","name":"感知任务4","description":"收集闵行校区内SSID为SJTU的WiFi信号强度"},
    	{"status":1,"contract":"0X4825811A5E63458953CFF8F5...","name":"感知任务5","description":"收集闵行校区内SSID为SJTU的WiFi信号强度"},
    	{"status":0,"contract":"0X4825811A5E63458953CFF8F5...","name":"感知任务6","description":"收集闵行校区内SSID为SJTU的WiFi信号强度"},
    	{"status":0,"contract":"0X4825811A5E63458953CFF8F5...","name":"感知任务7","description":"收集闵行校区内SSID为SJTU的WiFi信号强度"},
    	{"status":0,"contract":"0X4825811A5E63458953CFF8F5...","name":"感知任务8","description":"收集闵行校区内SSID为SJTU的WiFi信号强度"},
    	{"status":0,"contract":"0X4825811A5E63458953CFF8F5...","name":"感知任务9","description":"收集闵行校区内SSID为SJTU的WiFi信号强度"},
    	{"status":1,"contract":"0X4825811A5E63458953CFF8F5...","name":"感知任务10","description":"收集闵行校区内SSID为SJTU的WiFi信号强度"}
    ]
})

if (typeof web3 !== 'undefined') {
    web3 = new Web3(web3.currentProvider);
} else {
    // set the provider you want from Web3.providers
    web3 = new Web3(new Web3.providers.HttpProvider("http://localhost:8545"));
    web3.eth.defaultAccount = localStorage.getItem("defaultAccount");
}

var taskManager = ipcRenderer.sendSync('synchronous-taskManager');
console.log(taskManager)