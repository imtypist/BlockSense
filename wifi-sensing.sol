pragma solidity ^0.4.21;

contract SensingWifi{
    uint public rewardUnit;
    uint public rewardNum;
    uint public dataCount;
    bytes32 public wifiName;
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
    event TaskInited(uint ru, uint rn, bytes32 wn);
    event ViewTask(uint ru, uint rn, bytes32 wn, uint cnt);
    event DataCommited(bytes32 l, bytes32 w, int s);
    event TaskDone();
    event dataCheck(uint cnt);

    /// Init the task as requster.
    /// The requster need to set the value of reward for each sensing data,
    /// as well as the maximum number of data he needed.
    function initTask(uint _rewardUnit, uint _rewardNum, bytes32 _wifiName)
        public
        inState(State.Uncreated)
        condition(msg.value >= _rewardUnit * _rewardNum)
        payable
    {
        requester = msg.sender;
        rewardUnit = _rewardUnit;
        rewardNum = _rewardNum;
        wifiName = _wifiName;
        state = State.Created;
        emit TaskInited(_rewardUnit, _rewardNum, _wifiName);
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
    /// such as {"41-24-12.2-N 2-10-26.5-E", "CMCC", -51}
    
    function getTask()
        public
        inState(State.Created)
    {
        emit ViewTask(rewardUnit, rewardNum, wifiName, dataCount);
    }

    function commitTask(bytes32 _location, bytes32 _wifiName, int _signalDegree)
        public
        inState(State.Created)
    {
        require(dataCount < rewardNum);
        bytes memory sensingDataCommit = bytes(dataStatuses[_location]);
        // Requester wants to get data from different location
        require(sensingDataCommit.length == 0);
        
        // Make sure that the wifi signal sensed is what requester wants
        require(wifiName==_wifiName);
        
        // The theoretical maximum value of signal strength is -30 dBm
        require(_signalDegree < -30);
        
        dataStatuses[_location] = "Committed";
        
        dataCount += 1;
        if(dataCount == rewardNum){
            state = State.Inactive;
            requester.transfer(address(this).balance);
            emit TaskDone();
        }
        msg.sender.transfer(rewardUnit);
        emit DataCommited(_location, _wifiName, _signalDegree);
    }
}