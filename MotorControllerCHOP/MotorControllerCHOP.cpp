/* Shared Use License: This file is owned by Derivative Inc. (Derivative)
* and can only be used, and/or modified for use, in conjunction with
* Derivative's TouchDesigner software, and only if you are a licensee who has
* accepted Derivative's TouchDesigner license or assignment agreement
* (which also govern the use of this file). You may share or redistribute
* a modified version of this file provided the following conditions are met:
*
* 1. The shared file or redistribution must retain the information set out
* above and this list of conditions.
* 2. Derivative's name (Derivative Inc.) or its trademarks may not be used
* to endorse or promote products derived from this file without specific
* prior written permission from Derivative.
*/

#include "MotorControllerCHOP.h"

#include <stdio.h>
#include <string.h>
#include <cmath>
#include <assert.h>

// In case thread_sleep needed
#include <thread>
#include <chrono>

//// Library for ClearView and some constants
//#include "pubSysCls.h"
//using namespace sFnd;
//#define ACC_LIM_RPM_PER_SEC 100000
//#define VEL_LIM_RPM         700
//#define MOVE_DISTANCE_CNTS  10000   
//#define NUM_MOVES           5
//#define TIME_TILL_TIMEOUT   10000   //The timeout used for homing(ms)


// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{

DLLEXPORT
void
FillCHOPPluginInfo(CHOP_PluginInfo *info)
{
	// Always set this to CHOPCPlusPlusAPIVersion.
	info->apiVersion = CHOPCPlusPlusAPIVersion;

	// The opType is the unique name for this CHOP. It must start with a 
	// capital A-Z character, and all the following characters must lower case
	// or numbers (a-z, 0-9)
	info->customOPInfo.opType->setString("Motorcontroller");

	// The opLabel is the text that will show up in the OP Create Dialog
	info->customOPInfo.opLabel->setString("Motor Controller");

	// Information about the author of this OP
	info->customOPInfo.authorName->setString("Rifqi Dewangga");
	info->customOPInfo.authorEmail->setString("rifdewangga@gmail.com");

	// This CHOP can work with 0 inputs
	info->customOPInfo.minInputs = 0;

	// It can accept up to 1 input though, which changes it's behavior
	info->customOPInfo.maxInputs = 1;
}

DLLEXPORT
CHOP_CPlusPlusBase*
CreateCHOPInstance(const OP_NodeInfo* info)
{
	// Return a new instance of your class every time this is called.
	// It will be called once per CHOP that is using the .dll
	return new MotorControllerCHOP(info);
}

DLLEXPORT
void
DestroyCHOPInstance(CHOP_CPlusPlusBase* instance)
{
	// Delete the instance here, this will be called when
	// Touch is shutting down, when the CHOP using that instance is deleted, or
	// if the CHOP loads a different DLL
	delete (MotorControllerCHOP*)instance;
}

};


MotorControllerCHOP::MotorControllerCHOP(const OP_NodeInfo* info) : myNodeInfo(info)
{
}

MotorControllerCHOP::~MotorControllerCHOP()
{
}

void
MotorControllerCHOP::getGeneralInfo(CHOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved1)
{
	// This will cause the node to cook every frame
	ginfo->cookEveryFrameIfAsked = true;

	// Note: To disable timeslicing you'll need to turn this off, as well as ensure that
	// getOutputInfo() returns true, and likely also set the info->numSamples to how many
	// samples you want to generate for this CHOP. Otherwise it'll take on length of the
	// input CHOP, which may be timesliced.
	ginfo->timeslice = true;

	ginfo->inputMatchIndex = 0;
}

bool
MotorControllerCHOP::getOutputInfo(CHOP_OutputInfo* info, const OP_Inputs* inputs, void* reserved1)
{
	// If there is an input connected, we are going to match it's channel names etc
	// otherwise we'll specify our own.
	if (inputs->getNumInputs() > 0)
	{
		return false;
	}
	else
	{
		info->numChannels = 1;

		// Since we are outputting a timeslice, the system will dictate
		// the numSamples and startIndex of the CHOP data
		//info->numSamples = 1;
		//info->startIndex = 0

		// For illustration we are going to output 120hz data
		info->sampleRate = 120;
		return true;
	}
}

void
MotorControllerCHOP::getChannelName(int32_t index, OP_String *name, const OP_Inputs* inputs, void* reserved1)
{
	name->setString("MotorController");
}

void
MotorControllerCHOP::execute(CHOP_Output* output,
							  const OP_Inputs* inputs,
							  void* reserved)
{	
	iNode = inputs->getParInt("Inode");
	enable = inputs->getParInt("Enable");
	counts = inputs->getParDouble("Counts"); // in the real case this should ask from motor controller
	velocity = inputs->getParDouble("Velocity");
	acceleration = inputs->getParDouble("Acceleration");
}

int32_t
MotorControllerCHOP::getNumInfoCHOPChans(void * reserved1)
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the CHOP. In this example we are just going to send one channel.
	return 2;
}

void
MotorControllerCHOP::getInfoCHOPChan(int32_t index,
										OP_InfoCHOPChan* chan,
										void* reserved1)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.
}

bool		
MotorControllerCHOP::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1)
{
	infoSize->rows = 4;
	infoSize->cols = 8;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void
MotorControllerCHOP::getInfoDATEntries(int32_t index,
										int32_t nEntries,
										OP_InfoDATEntries* entries, 
										void* reserved1)
{
	char tempBuffer[4096];

	if (index == 0)
	{
		entries->values[0]->setString("iNode");
		entries->values[1]->setString("info");
		entries->values[2]->setString("enabled");
		entries->values[3]->setString("position (cnts)");
		entries->values[4]->setString("velocity (rpm)");
		entries->values[5]->setString("torque (..)");
		entries->values[6]->setString("reserved");
		entries->values[7]->setString("reserved");
	}

	if (index == 1)
	{
		for (size_t i = 0; i < 8; i++)
		{
			entries->values[i]->setString("");
		}
	}

	if (index == 2)
	{
		for (size_t i = 0; i < 8; i++)
		{
			entries->values[i]->setString("####");
		}
	}

	if (index == 3)
	{
		 //Set the value for the first column
		entries->values[0]->setString("debugoutput");

		 //Set the value
#ifdef _WIN32
		sprintf_s(tempBuffer, "%d", iNode);
		entries->values[1]->setString(tempBuffer);
		sprintf_s(tempBuffer, "%s", enable?"true":"false");
		entries->values[2]->setString(tempBuffer);
		sprintf_s(tempBuffer, "%.1f", counts);
		entries->values[3]->setString(tempBuffer);
		sprintf_s(tempBuffer, "%.1f", velocity);
		entries->values[4]->setString(tempBuffer);
		sprintf_s(tempBuffer, "%.1f", acceleration);
		entries->values[5]->setString(tempBuffer);
		sprintf_s(tempBuffer, "%d", nRotateClicked);
		entries->values[6]->setString(tempBuffer);
		entries->values[7]->setString("####");

#else // macOS, should add more to works on macOS
		snprintf(tempBuffer, sizeof(tempBuffer), "%d", myExecuteCount);
		entries->values[1]->setString(tempBuffer);
#endif
	}
}

void
MotorControllerCHOP::setupParameters(OP_ParameterManager* manager, void *reserved1)
{
	// iNode
	{
		OP_StringParameter	sp;

		sp.name = "Inode";
		sp.label = "Inode";
		sp.defaultValue = "0";

		const char* names[] = { "0", "1", "2", "3", "4", "5", "6", "7"};
		const char* labels[] = { "0", "1", "2", "3", "4", "5", "6", "7" };

		OP_ParAppendResult res = manager->appendMenu(sp, 8, names, labels);
		assert(res == OP_ParAppendResult::Success);
	}

	// enable/disable motor
	{
		OP_NumericParameter	np;

		np.name = "Enable";
		np.label = "Enable";
		np.defaultValues[0] = 0.0;

		OP_ParAppendResult res = manager->appendToggle(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// Counts
	{
		OP_NumericParameter	np;

		np.name = "Counts";
		np.label = "Counts";
		np.defaultValues[0] = 800.0;
		np.minSliders[0] = -100000.0;
		np.maxSliders[0] = 100000.0;

		OP_ParAppendResult res = manager->appendFloat(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// velocity
	{
		OP_NumericParameter	np;

		np.name = "Velocity";
		np.label = "Velocity";
		np.defaultValues[0] = 700.0;
		np.minSliders[0] = 0.0;
		np.maxSliders[0] =  1000.0;
		
		OP_ParAppendResult res = manager->appendFloat(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// acceleration
	{
		OP_NumericParameter	np;

		np.name = "Acceleration";
		np.label = "Acceleration";
		np.defaultValues[0] = 100000.0;
		np.minSliders[0] = 50000.0;
		np.maxSliders[0] = 150000.0;

		OP_ParAppendResult res = manager->appendFloat(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// rotate
	{
		OP_NumericParameter	np;

		np.name = "Rotate";
		np.label = "Rotate";

		OP_ParAppendResult res = manager->appendPulse(np);
		assert(res == OP_ParAppendResult::Success);
	}
}

void 
MotorControllerCHOP::pulsePressed(const char* name, void* reserved1)
{
	if (!strcmp(name, "Rotate"))
	{
		nRotateClicked++;
	}
}

/*
int MotorControllerCHOP::rotateMotor()
{
	size_t portCount = 0;
	std::vector<std::string> comHubPorts;

	//Create the SysManager object. This object will coordinate actions among various ports
	// and within nodes. In this example we use this object to setup and open our port.
	SysManager* myMgr = SysManager::Instance();							//Create System Manager myMgr

	//This will try to open the port. If there is an error/exception during the port opening,
	//the code will jump to the catch loop where detailed information regarding the error will be displayed;
	//otherwise the catch loop is skipped over
	try
	{
		SysManager::FindComHubPorts(comHubPorts);
		//printf("Found %d SC Hubs\n", comHubPorts.size());

		for (portCount = 0; portCount < comHubPorts.size() && portCount < NET_CONTROLLER_MAX; portCount++) {

			//define the first SC Hub port (port 0) to be associated 
			// with COM portnum (as seen in device manager)
			myMgr->ComHubPort(portCount, comHubPorts[portCount].c_str());
		}

		if (portCount < 0) {

			//printf("Unable to locate SC hub port\n");

			//msgUser("Press any key to continue."); //pause so the user can see the error message; waits for user to press a key

			return -1;  //This terminates the main program
		}

		//printf("\n I will now open port \t%i \n \n", portnum);
		myMgr->PortsOpen(portCount);				//Open the port

		for (size_t i = 0; i < portCount; i++) {
			IPort& myPort = myMgr->Ports(i);

			//printf(" Port[%d]: state=%d, nodes=%d\n", myPort.NetNumber(), myPort.OpenState(), myPort.NodeCount());

			//Once the code gets past this point, it can be assumed that the Port has been opened without issue
			//Now we can get a reference to our port object which we will use to access the node objects

			for (size_t iNode = 0; iNode < myPort.NodeCount(); iNode++) {
				// Create a shortcut reference for a node
				INode& theNode = myPort.Nodes(iNode);

				theNode.EnableReq(false);				//Ensure Node is disabled before loading config file

				myMgr->Delay(200);

				//theNode.Setup.ConfigLoad("Config File path");
				// printf("   Node[%d]: type=%d\n", int(iNode), theNode.Info.NodeType());
				// printf("            userID: %s\n", theNode.Info.UserID.Value());
				// printf("        FW version: %s\n", theNode.Info.FirmwareVersion.Value());
				// printf("          Serial #: %d\n", theNode.Info.SerialNumber.Value());
				// printf("             Model: %s\n", theNode.Info.Model.Value());

				//The following statements will attempt to enable the node.  First,
				// any shutdowns or NodeStops are cleared, finally the node is enabled
				theNode.Status.AlertsClear();					//Clear Alerts on node 
				theNode.Motion.NodeStopClear();	//Clear Nodestops on Node  				
				theNode.EnableReq(true);					//Enable node 
				//At this point the node is enabled
				//printf("Node \t%zi enabled\n", iNode);
				double timeout = myMgr->TimeStampMsec() + TIME_TILL_TIMEOUT;	//define a timeout in case the node is unable to enable
																			//This will loop checking on the Real time values of the node's Ready status
				while (!theNode.Motion.IsReady()) {
					if (myMgr->TimeStampMsec() > timeout) {
						//printf("Error: Timed out waiting for Node %d to enable\n", iNode);
						//msgUser("Press any key to continue."); //pause so the user can see the error message; waits for user to press a key
						return -2;
					}
				}
				//At this point the Node is enabled, and we will now check to see if the Node has been homed
				//Check the Node to see if it has already been homed, 
				if (theNode.Motion.Homing.HomingValid())
				{
					if (theNode.Motion.Homing.WasHomed())
					{
						//printf("Node %d has already been homed, current position is: \t%8.0f \n", iNode, theNode.Motion.PosnMeasured.Value());
						//printf("Rehoming Node... \n");
					}
					else
					{
						//printf("Node [%d] has not been homed.  Homing Node now...\n", iNode);
					}
					//Now we will home the Node
					theNode.Motion.Homing.Initiate();

					timeout = myMgr->TimeStampMsec() + TIME_TILL_TIMEOUT;	//define a timeout in case the node is unable to enable
																			// Basic mode - Poll until disabled
					while (!theNode.Motion.Homing.WasHomed()) {
						if (myMgr->TimeStampMsec() > timeout) {
							//printf("Node did not complete homing:  \n\t -Ensure Homing settings have been defined through ClearView. \n\t -Check for alerts/Shutdowns \n\t -Ensure timeout is longer than the longest possible homing move.\n");
							//msgUser("Press any key to continue."); //pause so the user can see the error message; waits for user to press a key
							return -2;
						}
					}
					//printf("Node completed homing\n");
				}
				else {
					//printf("Node[%d] has not had homing setup through ClearView.  The node will not be homed.\n", iNode);
				}

			}

			///////////////////////////////////////////////////////////////////////////////////////
			//At this point we will execute 10 rev moves sequentially on each axis
			//////////////////////////////////////////////////////////////////////////////////////

			for (size_t i = 0; i < NUM_MOVES; i++)
			{
				for (size_t iNode = 0; iNode < myPort.NodeCount(); iNode++) {
					// Create a shortcut reference for a node
					INode& theNode = myPort.Nodes(iNode);

					theNode.Motion.MoveWentDone();						//Clear the rising edge Move done register

					theNode.AccUnit(INode::RPM_PER_SEC);				//Set the units for Acceleration to RPM/SEC
					theNode.VelUnit(INode::RPM);						//Set the units for Velocity to RPM
					theNode.Motion.AccLimit = ACC_LIM_RPM_PER_SEC;		//Set Acceleration Limit (RPM/Sec)
					theNode.Motion.VelLimit = VEL_LIM_RPM;				//Set Velocity Limit (RPM)

					//printf("Moving Node \t%zi \n", iNode);
					theNode.Motion.MovePosnStart(MOVE_DISTANCE_CNTS);			//Execute 10000 encoder count move 
					//printf("%f estimated time.\n", theNode.Motion.MovePosnDurationMsec(MOVE_DISTANCE_CNTS));
					double timeout = myMgr->TimeStampMsec() + theNode.Motion.MovePosnDurationMsec(MOVE_DISTANCE_CNTS) + 100;			//define a timeout in case the node is unable to enable

					while (!theNode.Motion.MoveIsDone()) {
						if (myMgr->TimeStampMsec() > timeout) {
							//printf("Error: Timed out waiting for move to complete\n");
							//msgUser("Press any key to continue."); //pause so the user can see the error message; waits for user to press a key
							return -2;
						}
					}
					//printf("Node \t%zi Move Done\n", iNode);
				} // for each node
			} // for each move



		//////////////////////////////////////////////////////////////////////////////////////////////
		//After moves have completed Disable node, and close ports
		//////////////////////////////////////////////////////////////////////////////////////////////
			//printf("Disabling nodes, and closing port\n");
			//Disable Nodes

			for (size_t iNode = 0; iNode < myPort.NodeCount(); iNode++) {
				// Create a shortcut reference for a node
				myPort.Nodes(iNode).EnableReq(false);
			}
		}
	}
	catch (mnErr& theErr)
	{
		//printf("Failed to disable Nodes n\n");
		//This statement will print the address of the error, the error code (defined by the mnErr class), 
		//as well as the corresponding error message.
		//printf("Caught error: addr=%d, err=0x%08x\nmsg=%s\n", theErr.TheAddr, theErr.ErrorCode, theErr.ErrorMsg);

		//msgUser("Press any key to continue."); //pause so the user can see the error message; waits for user to press a key
		return 0;  //This terminates the main program
	}

	// Close down the ports
	myMgr->PortsClose();

	//msgUser("Press any key to continue."); //pause so the user can see the error message; waits for user to press a key
	
	return 0;
}
*/
