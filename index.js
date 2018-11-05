const {app, BrowserWindow} = require('electron')
  
// Keep a global reference of the window object, if you don't, the window will
// be closed automatically when the JavaScript object is garbage collected.
let win

function createWindow () {
	webPreferences: {
        nodeIntegration: false
    }
	// 创建浏览器窗口。
	win = new BrowserWindow({width: 1200, height: 700, resizable: false})

	// 然后加载应用的 index.html。
	win.loadFile('index.html')

	win.setMenu(null)

	// 打开开发者工具
	win.webContents.openDevTools()

	// 当 window 被关闭，这个事件会被触发。
	win.on('closed', () => {
	  // 取消引用 window 对象，如果你的应用支持多窗口的话，
	  // 通常会把多个 window 对象存放在一个数组里面，
	  // 与此同时，你应该删除相应的元素。
	  win = null
	})
}

// Electron 会在初始化后并准备
// 创建浏览器窗口时，调用这个函数。
// 部分 API 在 ready 事件触发后才能使用。
app.on('ready', createWindow)

// 当全部窗口关闭时退出。
app.on('window-all-closed', () => {
	// 在 macOS 上，除非用户用 Cmd + Q 确定地退出，
	// 否则绝大部分应用及其菜单栏会保持激活。
	if (process.platform !== 'darwin') {
	  app.quit()
	}
})

app.on('activate', () => {
	// 在macOS上，当单击dock图标并且没有其他窗口打开时，
	// 通常在应用程序中重新创建一个窗口。
	if (win === null) {
	  createWindow()
	}
})

// 在主进程中.
const {ipcMain} = require('electron')
// https://electronjs.org/docs/api/ipc-main
  
ipcMain.on('synchronous-addSensedData', (event, arg) => {
  // console.log(arg)
  addSensedData(arg).then(dataHash =>{
  	event.returnValue = dataHash;
  })
})

ipcMain.on('synchronous-catSensedData', (event, arg) => {
  // console.log(arg)
  catSensedData(arg).then(fileBuffer =>{
  	event.returnValue = fileBuffer;
  })
})

// IPFS & solc
const fs = require('fs')
const solc = require('solc')

const IPFS = require('ipfs')
const node = new IPFS()

node.on('ready', async () => {
  const version = await node.version();
  console.log('Version:', version.version);
});

// 存数据
async function addSensedData(sensedData){
  const filesAdded = await node.files.add({
    content: Buffer.from(sensedData)
  })
  console.log('Added file:', filesAdded[0].path, filesAdded[0].hash)
  return filesAdded[0].hash;
}

// 取数据
async function catSensedData(ipfsHash){
  const fileBuffer = await node.files.cat(ipfsHash);
  console.log('Added file contents:', fileBuffer.toString())
  return fileBuffer.toString();
}

var Web3 = require("web3");
// 创建web3对象
var web3 = new Web3();
// 连接到本地节点
web3.setProvider(new Web3.providers.HttpProvider("http://localhost:8545"));

function deployManagerContract(){
	let source = fs.readFileSync('dist/contracts/taskManagement.sol', 'utf8');
	let compiledContract = solc.compile(source, 1);
	let abi = compiledContract.contracts['taskManagement'].interface;
	let bytecode = compiledContract.contracts['taskManagement'].bytecode;
	let gasEstimate = web3.eth.estimateGas({data: bytecode});
	let MyContract = web3.eth.contract(JSON.parse(abi));
	localStorage.setItem('taskManagementContractAbi',JSON.parse(abi));
	var myContractReturned = MyContract.new({
	  from:web3.eth.accounts[0],
	  data:bytecode,
	  gas:gasEstimate}, function(err, myContract){
	   if(!err) {
	      // NOTE: The callback will fire twice!
	      // Once the contract has the transactionHash property set and once its deployed on an address.
	      if(!myContract.address) {
	          console.log(myContract.transactionHash) // The hash of the transaction, which deploys the contract
	      } else {
	          console.log(myContract.address) // the contract address
	          localStorage.setItem('taskManagementContractAddress',myContract.address);
	      }
	   }
	 });
}

deployManagerContract();