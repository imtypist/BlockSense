const { app, BrowserWindow } = require('electron')

// Keep a global reference of the window object, if you don't, the window will
// be closed automatically when the JavaScript object is garbage collected.
let win

function createWindow () {
  webPreferences: {
    nodeIntegration: false
  }
  // Create the browser window.
  win = new BrowserWindow({ width: 1200, height: 700, resizable: false, icon: 'dist/img/crowd.png'})

  // and load the index.html of the app.
  win.loadFile('index.html')

  // Open the DevTools.
  win.webContents.openDevTools()

  // Emitted when the window is closed.
  win.on('closed', () => {
    // Dereference the window object, usually you would store windows
    // in an array if your app supports multi windows, this is the time
    // when you should delete the corresponding element.
    win = null
  })
}

// This method will be called when Electron has finished
// initialization and is ready to create browser windows.
// Some APIs can only be used after this event occurs.
app.on('ready', createWindow)

// Quit when all windows are closed.
app.on('window-all-closed', () => {
  // On macOS it is common for applications and their menu bar
  // to stay active until the user quits explicitly with Cmd + Q
  if (process.platform !== 'darwin') {
    app.quit()
  }
})

app.on('activate', () => {
  // On macOS it's common to re-create a window in the app when the
  // dock icon is clicked and there are no other windows open.
  if (win === null) {
    createWindow()
  }
})

// In this file you can include the rest of your app's specific main process
// code. You can also put them in separate files and require them here.
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

// save data
async function addSensedData(sensedData){
  const filesAdded = await node.files.add({
    content: Buffer.from(sensedData)
  })
  console.log('Added file:', filesAdded[0].path, filesAdded[0].hash)
  return filesAdded[0].hash;
}

// retrieve data
async function catSensedData(ipfsHash){
  const fileBuffer = await node.files.cat(ipfsHash);
  console.log('Added file contents:', fileBuffer.toString())
  return fileBuffer.toString();
}

// IPFS & solc
const fs = require('fs')
const solc = require('solc')
const IPFS = require('ipfs')
const node = new IPFS()

node.on('ready', async () => {
  const version = await node.version();
  console.log('Version:', version.version);
});

var Web3 = require("web3");
var web3 = new Web3();
web3.setProvider(new Web3.providers.HttpProvider("http://localhost:8545"));

if (typeof localStorage === "undefined" || localStorage === null) {
  var LocalStorage = require('node-localstorage').LocalStorage;
  localStorage = new LocalStorage('./scratch');
}

function compileContract(contract_name){
  let source = fs.readFileSync("./dist/contracts/" + contract_name + ".sol", 'utf8')
  console.log('=> compiling contract ' + contract_name);
  let compiledContract;
  try{
    compiledContract = solc.compile(source);
  }catch(err){
    console.log(err);
    return -1;
  }
  for (let contractName in compiledContract.contracts) {
    var bytecode = compiledContract.contracts[contractName].bytecode;
    var abi = compiledContract.contracts[contractName].interface;
  }
  contract_info = {"abi": abi, "bytecode": bytecode};
  return contract_info;
}

function deployManagerContract(){
  let contract_info = compileContract('taskManagement');
  let bytecode = contract_info.bytecode;
  localStorage.setItem('_abi',contract_info.abi);
  let abi = JSON.parse(contract_info.abi);
  web3.eth.estimateGas({data: bytecode}).then(function(gasEstimate){
    web3.eth.getAccounts().then(function(accounts){
      let myContract = new web3.eth.Contract(abi,null,{from: accounts[0], gas: gasEstimate});
      myContract.deploy({data: bytecode})
      .send({from: accounts[0], gas: gasEstimate},function(error, transactionHash){
        console.log("=> hash: " + transactionHash)
      })
      .then(function(newContractInstance){
        console.log("=> address: " + newContractInstance.options.address) // instance with the new contract address
        localStorage.setItem('_address', newContractInstance.options.address);
      });
    })
  });
}

if(!(localStorage.getItem('_abi') && localStorage.getItem('_address'))){
  deployManagerContract();
}

ipcMain.on('synchronous-taskManager', (event, arg) => {
  taskManager().then(result =>{
    event.returnValue = result;
  })
})

async function taskManager(){
  contract_info = {"abi": localStorage.getItem('_abi'), "address": localStorage.getItem('_address')};
  return contract_info;
}

ipcMain.on('synchronous-compileTask', (event, arg) => {
  // console.log(arg)
  compileTask(arg).then(res =>{
    event.returnValue = res;
  })
})

async function compileTask(condition){ // string type
  let source = fs.readFileSync("./dist/contracts/sensingTaskTemplate.sol", 'utf8')
  var reg = /@condition/;
  source = source.replace(reg, condition);
  let compiledContract;
  try{
    compiledContract = solc.compile(source);
  }catch(err){
    console.log(err);
    return -1; // compile failed
  }
  for (let contractName in compiledContract.contracts) {
    var bytecode = compiledContract.contracts[contractName].bytecode;
    var abi = compiledContract.contracts[contractName].interface;
  }
  contract_info = {"abi": abi, "bytecode": bytecode};
  return JSON.stringify(contract_info);
}