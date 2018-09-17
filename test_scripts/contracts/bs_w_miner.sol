pragma solidity ^0.4.21;

contract BlockSense{

    mapping(string => string) dataStatuses;

    modifier condition(bool _condition) {
        require(_condition);
        _;
    }

    function commitTask(string _location, string data, int _signalDegree)
        public
    {
        bytes memory sensingDataCommit = bytes(dataStatuses[_location]);
        // Requester wants to get data from different location
        require(sensingDataCommit.length == 0);
        
        // The theoretical maximum value of signal strength is -30 dBm
        require(_signalDegree < -30);
        
        dataStatuses[_location] = data;
    }
}