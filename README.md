# BlockSense — A Blockchain-based Decentralized Crowd-sensing System

### Dependencies

- nodejs@v8.11.1

- python@v2.7.15

- ganache@v1.2.2

### Installation

```bash
$ npm install
```

And rebuild some modules for electron,

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

### Some errors may happen

> **Q1:** Error: A dynamic link library (DLL) initialization routine failed
>
> **A1:** [https://github.com/electron/electron/issues/10193](https://github.com/electron/electron/issues/10193)、
     [https://electronjs.org/docs/tutorial/using-native-node-modules](https://electronjs.org/docs/tutorial/using-native-node-modules)
     以及只有在`dependencies`中的modules才会被rebuild，还是失败的话检查一下package.json文件有没有写对

> **Q2:** How to use `sloc` and `js-ipfs` modules with Electron
>
> **A2:** [https://www.npmjs.com/package/solc#using-with-electron](https://www.npmjs.com/package/solc#using-with-electron)、
     [https://electronjs.org/docs/faq](https://electronjs.org/docs/faq)

> **Q3:** npm install build `ERROR MSB4019`
>
> **A3:** [https://stackoverflow.com/questions/18774929/npm-install-g-karma-error-msb4019-the-imported-project-c-microsoft-cpp-defau](https://stackoverflow.com/questions/18774929/npm-install-g-karma-error-msb4019-the-imported-project-c-microsoft-cpp-defau)

> **Q4:** An unhandled error occurred inside electron-rebuild
>
> **A4:** [https://github.com/Microsoft/nodejs-guidelines/blob/master/windows-environment.md#compiling-native-addon-modules](https://github.com/Microsoft/nodejs-guidelines/blob/master/windows-environment.md#compiling-native-addon-modules)、[https://github.com/nodejs/node-gyp#installation](https://github.com/nodejs/node-gyp#installation)
     检查路径中不要有中文. I tried it on Windows and Ubuntu, which indicated that the same error occur on both platforms, i.e., `openssl/rsa.h: No such file`. For Windows, I haven't found a solution, but it is easy to solve on Ubuntu, which just needs a simple command, `sudo apt-get install libssl-dev`.  Waste much time on it!

> **Q5:** Sweetalert2 text input not clickable when bootstrap modal is open
>
> **A5:** [https://github.com/sweetalert2/sweetalert2/issues/374](https://github.com/sweetalert2/sweetalert2/issues/374)

> **Q6:** Raise the `exceed gas limit` error
>
> **A6:** Just increase the upper bound of gas limit for testrpc or geth client, here I set limit gas to 100000000.