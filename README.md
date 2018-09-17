# BlockSense — A Blockchain-based Decentralized Crowd-sensing System
Here we use a simple wifi-sensing task as an instance to show how our framework works.

#### Installation:

1. Configure [MetaMask](https://karl.tech/learning-solidity-part-1-deploy-a-contract/) in google chrome.
2. Install web3.js, detailed at [official documents](https://github.com/ethereum/wiki/wiki/JavaScript-API).

Then, in order to achieve the interaction between web3.js and smart contract, we need to build a virtualized server for local webpages. Here, we use lite-server to achieve this goal, which can be installed by: 

3. Input text in Command Line under the folder of web.js：

```
>> npm install lite-server --save-dev
```

**Notes**: If web.js is installed globally in the prior operation, the user needs to text in`npm init` first.

4. open  **package.json**, add text below in`scripts`:

```
"scripts": {    
    "dev": "lite-server"
  },
```

5. If you want to play the role as the requester, you need to rename **index-requester.html** as **index.html** first; So as the worker, you need to rename **index-worker.html** as **index.html** first.
6. After we get the **index.html** under current directory, text code below in the Command Line：

```
>> npm run dev
```

**Important:** You can find more detailed configuration at  [Youtube Video](https://coursetro.com/posts/code/103/Solidity-Inheritance-and-Deploying-an-Ethereum-Smart-Contract).
