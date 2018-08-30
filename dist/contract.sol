pragma solidity ^0.4.21;

contract @contractname{
    uint public rewardUnit;
    uint public rewardNum;
    uint public dataCount;
    address public requester;
    enum State {Uncreated, Created, Inactive}
    State public state;

    mapping(bytes32 => string) dataStatuses; // Either '' or 'Committed'

    modifier condition(bool _condition) {
        require(_condition);
        _;
    }

    modifier onlyRequester() {
        require(msg.sender == requester);
        _;
    }

    modifier inState(State _state) {
        require(state == _state);
        _;
    }

    event Aborted();
    event TaskInited(uint ru, uint rn, @initTask);
    event ViewTask(uint ru, uint rn, bytes32 wn, uint cnt);
    event DataCommited(bytes32 l, @commitTask);
    event TaskDone();
    event dataCheck(uint cnt);

    /// Init the task as requster.
    /// The requster need to set the value of reward for each sensing data,
    /// as well as the maximum number of data he needed.
    function initTask(uint _rewardUnit, uint _rewardNum, @initTask)
        public
        inState(State.Uncreated)
        condition(msg.value >= _rewardUnit * _rewardNum)
        payable
    {
        requester = msg.sender;
        rewardUnit = _rewardUnit;
        rewardNum = _rewardNum;
        state = State.Created;
        _;
        emit TaskInited(_rewardUnit, _rewardNum, @taskInited);
    }

    /// Abort the Task and reclaim the ether,
    /// Can only be called by the requester.
    function abort()
        public
        onlyRequester
        inState(State.Created)
    {
        require(dataCount <= rewardNum);
        state = State.Inactive;
        requester.transfer(address(this).balance);
        emit Aborted();
    }
    
    function getDataCnt()
        public
        onlyRequester
        inState(State.Created)
    {
        emit dataCheck(dataCount);
    }
    
    /// Worker answer the task by sending data
    
    function getTask()
        public
        inState(State.Created)
    {
        emit ViewTask(rewardUnit, rewardNum, @initTask, dataCount);
    }

    function commitTask(bytes32 metadata, @commitTask)
        public
        inState(State.Created)
    {
        require(dataCount < rewardNum);
        bytes memory sensingDataCommit = bytes(dataStatuses[metadata]);

        require(sensingDataCommit.length == 0);
        
        require(@condition);
        
        dataStatuses[metadata] = "Committed";
        
        dataCount += 1;
        if(dataCount == rewardNum){
            state = State.Inactive;
            requester.transfer(address(this).balance);
            emit TaskDone();
        }
        msg.sender.transfer(rewardUnit);
        emit DataCommited(metadata, @dataCommited);
    }
}