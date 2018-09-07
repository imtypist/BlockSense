var taskList = avalon.define({
    $id: "taskList",
    data: [
    	{"status":0,"contract":"0x51a20e7b04b6aab7f8f7ff3f...","name":"感知任务1","description":"收集闵行校区内SSID为SJTU的WiFi信号强度"},
        {"status":0,"contract":"0X4825811A5E63458953CFF8F5...","name":"感知任务3","description":"收集闵行校区内SSID为SJTU的WiFi信号强度"},
        {"status":0,"contract":"0X4825811A5E63458953CFF8F5...","name":"感知任务9","description":"收集闵行校区内SSID为SJTU的WiFi信号强度"},
    ]
})

var ipfs = window.IpfsApi('127.0.0.1', '5002');
var Buffer = window.IpfsApi().Buffer;

ipfs.add(Buffer.from("123")).then(function(err,res){
	console.log(res);
})
