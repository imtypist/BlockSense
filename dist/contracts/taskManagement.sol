pragma solidity ^0.4.24;

contract taskManagement{
	struct taskState{
		address requester;
		uint state;
	}

	mapping(address => taskState) states;

    event taskList(address contra, string taskabi, string taskName, string taskDescrip);

    function addTask(address contra, string taskabi, string taskName, string taskDescrip)
    	public
    {
    	require(states[contra].state == 0);
    	//state => 1:ACTIVE 2:ABORT 3:COMPLETE
    	states[contra] = taskState(msg.sender, 1);
    	emit taskList(contra, taskabi, taskName, taskDescrip);
    }

    event stateChanged();

    function changeState(address contra, uint _state)
    	public
    {
    	require(states[contra].state == 1 && msg.sender == states[contra].requester);
    	states[contra].state = _state;
    	emit stateChanged();
    }
}