pragma solidity ^0.4.21;

contract CrowdSense{

    mapping(string => string) dataStatuses;

    function commitTask(string _location, string data, int _signalDegree)
        public
    {
        bytes memory sensingDataCommit = bytes(dataStatuses[_location]);
        
        dataStatuses[_location] = data;
    }
}