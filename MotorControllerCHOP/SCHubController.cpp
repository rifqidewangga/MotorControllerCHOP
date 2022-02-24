#include "SCHubController.h"

SCHubController::SCHubController()
{
	initializePort();
}

SCHubController::~SCHubController()
{
	_myMgr->PortsClose();
}

int SCHubController::initializePort()
{
	size_t portCount = 0;
	std::vector<std::string> comHubPorts;

	_myMgr = SysManager::Instance();

	SysManager::FindComHubPorts(comHubPorts);

	for (portCount = 0; portCount < comHubPorts.size() && portCount < NET_CONTROLLER_MAX; portCount++) {
		_myMgr->ComHubPort(portCount, comHubPorts[portCount].c_str());
	}

	if (portCount < 0) {
		return Status::PORT_NOT_FOUND;  //This terminates the main program
	}
	
	_myMgr->PortsOpen(portCount);

	return Status::SUCCESS;
}

void SCHubController::enableMotor(size_t iNode, bool newState)
{
	IPort& myPort = _myMgr->Ports(_portID);

	// Once the code gets past this point, it can be assumed that the Port has been opened without issue
	// Now we can get a reference to our port object which we will use to access the node objects

	INode& theNode = myPort.Nodes(iNode);

	theNode.EnableReq(newState);
}

bool SCHubController::enableMotor(size_t iNode)
{
	IPort& myPort = _myMgr->Ports(_portID);

	//Once the code gets past this point, it can be assumed that the Port has been opened without issue
	//Now we can get a reference to our port object which we will use to access the node objects

	INode& theNode = myPort.Nodes(iNode);

	return theNode.EnableReq();
}

int SCHubController::rotateMotor(size_t iNode, int32_t distanceCnts, double velLimit, double accLimit)
{
	try
	{
		IPort& myPort = _myMgr->Ports(_portID);

		//Once the code gets past this point, it can be assumed that the Port has been opened without issue
		//Now we can get a reference to our port object which we will use to access the node objects

		INode& theNode = myPort.Nodes(iNode);

		theNode.EnableReq(false);

		_myMgr->Delay(200);

		theNode.Status.AlertsClear();  // Clear Alerts on node 
		theNode.Motion.NodeStopClear();	// Clear Nodestops on Node  				
		theNode.EnableReq(true);  // Enable node 

		double timeout = _myMgr->TimeStampMsec() + DEFAULT_TIME_TILL_TIMEOUT;

		while (!theNode.Motion.IsReady()) {
			if (_myMgr->TimeStampMsec() > timeout) {
				return Status::TIMEOUT;
			}
		}

		// Check if the node has valid homing setup
		if (theNode.Motion.Homing.HomingValid())
		{
			theNode.Motion.Homing.Initiate();

			timeout = _myMgr->TimeStampMsec() + DEFAULT_TIME_TILL_TIMEOUT;	//define a timeout in case the node is unable to enable
																	// Basic mode - Poll until disabled
			while (!theNode.Motion.Homing.WasHomed()) {
				if (_myMgr->TimeStampMsec() > timeout) {
					//printf("Node did not complete homing:  \n\t -Ensure Homing settings have been defined through ClearView. \n\t -Check for alerts/Shutdowns \n\t -Ensure timeout is longer than the longest possible homing move.\n");
					//msgUser("Press any key to continue."); //pause so the user can see the error message; waits for user to press a key
					return Status::HOMING_TIMEOUT;
				}
			}
		}
		else {
			//printf("Node[%d] has not had homing setup through ClearView.  The node will not be homed.\n", iNode);
		}

		theNode.Motion.MoveWentDone();  // Clear the rising edge Move done register

		theNode.AccUnit(INode::RPM_PER_SEC);
		theNode.VelUnit(INode::RPM);
		theNode.Motion.AccLimit = accLimit;
		theNode.Motion.VelLimit = velLimit;

		//printf("Moving Node \t%zi \n", iNode);
		theNode.Motion.MovePosnStart(distanceCnts);			//Execute 10000 encoder count move
		//printf("%f estimated time.\n", theNode.Motion.MovePosnDurationMsec(MOVE_DISTANCE_CNTS));
		timeout = _myMgr->TimeStampMsec() + theNode.Motion.MovePosnDurationMsec(distanceCnts) + 100;			//define a timeout in case the node is unable to enable

		while (!theNode.Motion.MoveIsDone()) {
			if (_myMgr->TimeStampMsec() > timeout) {
				//printf("Error: Timed out waiting for move to complete\n");
				//msgUser("Press any key to continue."); //pause so the user can see the error message; waits for user to press a key
				return Status::TIMEOUT;
			}
		}

		theNode.EnableReq(false);
	}
	catch (mnErr& theErr)
	{
		//printf("Failed to disable Nodes n\n");
		//This statement will print the address of the error, the error code (defined by the mnErr class), 
		//as well as the corresponding error message.
		//printf("Caught error: addr=%d, err=0x%08x\nmsg=%s\n", theErr.TheAddr, theErr.ErrorCode, theErr.ErrorMsg);

		//msgUser("Press any key to continue."); //pause so the user can see the error message; waits for user to press a key
		return Status::ERROR_CONTROLLER;  //This terminates the main program
	}

	return Status::SUCCESS;
}