# BlockSense — A Blockchain-based Decentralized Crowd-sensing System

### Dependencies

- nodejs@v8.11.1
- python@v2.7.15

### Installation

```bash
$ npm install
```

And rebuild some modules for electron,

```bash
$ .\node_modules\.bin\electron-rebuild.cmd
```

### Run it

```bash
$ npm start
```

### FAQ

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