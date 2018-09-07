pragma solidity ^0.4.24;

contract @contractname{
    uint public rewardUnit;
    uint public rewardNum;
    uint public dataCount;
    address public requester;
    uint public state; // 1:ACTIVE 2:ABORT 3:COMPLETE
    string public taskDescrip;

    mapping(bytes32 => string) dataStatus; // Either '' or 'Committed'

    modifier condition(bool _condition) {
        require(_condition);
        _;
    }

    modifier onlyRequester() {
        require(msg.sender == requester);
        _;
    }

    modifier inState(uint _state) {
        require(state == _state);
        _;
    }

    // construct function
    event TaskInited(uint rewardUnit, uint rewardNum, string taskDescrip);

    constructor(uint _rewardUnit, uint _rewardNum, string _taskDescrip)
        public
        condition(msg.value >= _rewardUnit * _rewardNum)
        payable
    {
        requester = msg.sender;
        rewardUnit = _rewardUnit;
        rewardNum = _rewardNum;
        taskDescrip = _taskDescrip;
        state = 1; // ACTIVE
        // customized area

        emit TaskInited(_rewardUnit, _rewardNum, _taskDescrip);
    }

    // abort task
    event Aborted();

    function abort()
        public
        onlyRequester
        inState(1)
        condition(dataCount <= rewardNum)
    {
        state = 2; // ABORT
        requester.transfer(address(this).balance);
        emit Aborted();
    }
    
    // workers commit sensing data
    event DataCommited(bytes32 location, int sensingData);
    event TaskDone();

    function commitTask(bytes32 _location, int sensingData)
        public
        inState(1)
        condition(dataCount < rewardNum)
    {
        bytes memory sensingDataCommit = bytes(dataStatus[_location]);
        require(sensingDataCommit.length == 0);
        
        require(sensingData @dataCheck);
        
        dataStatus[_location] = "Committed";
        
        msg.sender.transfer(rewardUnit);
        emit DataCommited(_location, sensingData);
        
        dataCount += 1;
        if(dataCount == rewardNum){
            state = 3; // COMPLETE
            requester.transfer(address(this).balance);
            emit TaskDone();
        }
    }
}