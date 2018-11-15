# BlockSense — A Blockchain-based Decentralized Crowd-sensing System

### Environment

```
+ nodejs@v8.11.1
+ python@v2.7.15
+ ganache@v1.2.2
```

### Installation

```bash
$ npm install
```

**=> Note that** you need to rebuild modules for electron after pre-step

```bash
# for windows
$ .\node_modules\.bin\electron-rebuild.cmd
# for linux
$ ./node_modules/.bin/electron-rebuild
```

### Run it

```bash
$ npm start
```

### Possible errors

- **Error: A dynamic link library (DLL) initialization routine failed**

两个可能有用的链接：[issues#10193](https://github.com/electron/electron/issues/10193), [using-native-node-modules](https://electronjs.org/docs/tutorial/using-native-node-modules), 注意只有在`dependencies`中的modules才会被rebuild，还是失败的话检查一下package.json文件有没有写对

- **How to use `sloc` and `js-ipfs` modules with Electron**

两个可能有用的链接：[solc#using-with-electron](https://www.npmjs.com/package/solc#using-with-electron), [electronjs.org/docs/faq](https://electronjs.org/docs/faq)

- **npm install build `ERROR MSB4019`**

可以参考这个链接：[npm-install-g-karma-error-msb4019-the-imported-project-c-microsoft-cpp-defau](https://stackoverflow.com/questions/18774929/npm-install-g-karma-error-msb4019-the-imported-project-c-microsoft-cpp-defau)

- **An unhandled error occurred inside electron-rebuild**

两个可能有用的链接：[compiling-native-addon-modules](https://github.com/Microsoft/nodejs-guidelines/blob/master/windows-environment.md#compiling-native-addon-modules),[node-gyp#installation](https://github.com/nodejs/node-gyp#installation), 另外检查路径中不要有中文。

I tried it on Windows and Ubuntu, which indicated that the same error occur on both platforms, i.e., `openssl/rsa.h: No such file`. For Windows, I haven't found a solution, but it is easy to solve on Ubuntu, which just needs a simple command, `sudo apt-get install libssl-dev`.  Waste much time on it!

- **Sweetalert2 text input not clickable/focus when bootstrap modal is open**

This link is helpful to solve it: [issues#374](https://github.com/sweetalert2/sweetalert2/issues/374)

- **Raise the `exceed gas limit` error**

Just increase the upper bound of gas limit for testrpc or geth client, here I set limit gas to 100000000.
