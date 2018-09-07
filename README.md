# BlockSense — A Blockchain-based Decentralized Crowd-sensing System

#### Installation

```bash
$ npm install
```

#### Run it

```bash
$ npm start
```

#### FAQ

```bash
Q: Error: A dynamic link library (DLL) initialization routine failed
A: https://github.com/electron/electron/issues/10193
   https://electronjs.org/docs/tutorial/using-native-node-modules (另外补充：只有在"dependencies"中的modules才会被rebuild，所以失败的同学检查一下package.json文件)

Q: sloc and js-ipfs modules using with Electron
A: https://www.npmjs.com/package/solc#using-with-electron
   https://electronjs.org/docs/faq
```