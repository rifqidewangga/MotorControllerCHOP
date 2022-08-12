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
	info->customOPInfo.maxInputs = 16;
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
	updateNodeCount();
}

MotorControllerCHOP::~MotorControllerCHOP()
{
}

void
MotorControllerCHOP::getGeneralInfo(CHOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved1)
{
	// This will cause the node not to cook every frame
	ginfo->cookEveryFrameIfAsked = true;

	// Note: To disable timeslicing you'll need to turn this off, as well as ensure that
	// getOutputInfo() returns true, and likely also set the info->numSamples to how many
	// samples you want to generate for this CHOP. Otherwise it'll take on length of the
	// input CHOP, which may be timesliced.
	ginfo->timeslice = false;

	ginfo->inputMatchIndex = 0;
}

bool
MotorControllerCHOP::getOutputInfo(CHOP_OutputInfo* info, const OP_Inputs* inputs, void* reserved1)
{
	return true;
}

void
MotorControllerCHOP::getChannelName(int32_t index, OP_String *name, const OP_Inputs* inputs, void* reserved1)
{
	name->setString("chan1");
}

void
MotorControllerCHOP::execute(CHOP_Output* output,
							  const OP_Inputs* inputs,
							  void* reserved)
{	
	updateNodeCount();
	updateMotorCommands(inputs);
	sendMotorCommands(inputs);
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
	infoSize->rows = 18;
	infoSize->cols = 9;
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
		fillNodeHeader(entries);

	if (index > 0 && index < 17)
		fillNodeInfo(entries, index - 1);

	if (index == 17)
		fillDebugInfo(entries);
}

void
MotorControllerCHOP::setupParameters(OP_ParameterManager* manager, void *reserved1)
{
}

void 
MotorControllerCHOP::pulsePressed(const char* name, void* reserved1)
{
}

void MotorControllerCHOP::updateNodeCount()
{
#ifndef SIMULATION
	nodeCount = motorController.getNodeCount();
#else
	nodeCount = 2;
#endif // !SIMULATION
}

void MotorControllerCHOP::updateMotorCommand(const OP_Inputs* inputs, int iNode)
{
	motorsInfo[iNode].CmpPos = inputs->getInputCHOP(iNode)->channelData[0][0];
	motorsInfo[iNode].CmdVel = inputs->getInputCHOP(iNode)->channelData[1][0];
	motorsInfo[iNode].CmdAcc = inputs->getInputCHOP(iNode)->channelData[2][0];

#ifndef SIMULATION
	motorsInfo[iNode].IsEnable		= motorController.enableMotor(iNode);

	motorsInfo[iNode].MeasuredPos	= motorController.getMeasuredPos(iNode);
	motorsInfo[iNode].MeasuredVel	= motorController.getMeasuredVel(iNode);
	motorsInfo[iNode].MeasuredTrq	= motorController.getMeasuredTrq(iNode);
#else
	motorsInfo[iNode].IsEnable		= false;

	motorsInfo[iNode].MeasuredPos	= 0.0;
	motorsInfo[iNode].MeasuredVel	= 0.0;
	motorsInfo[iNode].MeasuredTrq	= 0.0;
#endif // !SIMULATION
}

void MotorControllerCHOP::updateMotorCommands(const OP_Inputs* inputs)
{
	size_t availableNode = 0;
	auto numInput = inputs->getNumInputs();

	availableNode = (numInput < nodeCount) ? numInput : nodeCount;
	
	for (size_t i = 0; i < availableNode; i++)
	{
		updateMotorCommand(inputs, i);
	}
}

void MotorControllerCHOP::sendMotorCommand(int iNode)
{
	if (isNodeAvailable(iNode))
	{
#ifndef SIMULATION
		auto cmd = motorsInfo[iNode];
		motorController.rotateMotor(iNode, cmd.CmpPos, cmd.CmdVel, cmd.CmdAcc);
#endif // !SIMULATION
	}
}

void MotorControllerCHOP::sendMotorCommands(const OP_Inputs* inputs)
{
	size_t availableNode = 0;
	auto numInput = inputs->getNumInputs();

	availableNode = (numInput < nodeCount) ? numInput : nodeCount;

	for (size_t i = 0; i < availableNode; i++)
	{
		sendMotorCommand(i);
	}
}

bool MotorControllerCHOP::isNodeAvailable(int iNode)
{
	return iNode < nodeCount ? true : false;
}

void MotorControllerCHOP::fillNodeHeader(OP_InfoDATEntries* entries)
{
	entries->values[0]->setString("iNode");
	entries->values[1]->setString("info");
	entries->values[2]->setString("enabled");
	entries->values[3]->setString("cmd_position (cnts)");
	entries->values[4]->setString("cmd_velocity (rpm)");
	entries->values[5]->setString("cmd_acceleration (rpm/s)");
	entries->values[6]->setString("positions (cnts)");
	entries->values[7]->setString("velocity (rpm)");
	entries->values[8]->setString("torque (% MAX)");
}

void MotorControllerCHOP::fillNodeInfo(OP_InfoDATEntries* entries, int iNode)
{
	std::string temp;

	temp = std::to_string(iNode);
	entries->values[0]->setString(temp.c_str());

	if (isNodeAvailable(iNode)) {
		temp = "Available";
		entries->values[1]->setString(temp.c_str());

		temp = std::to_string(motorsInfo[iNode].IsEnable);
		entries->values[2]->setString(temp.c_str());
		
		temp = std::to_string(motorsInfo[iNode].CmpPos);
		entries->values[3]->setString(temp.c_str());

		temp = std::to_string(motorsInfo[iNode].CmdVel);
		entries->values[4]->setString(temp.c_str());

		temp = std::to_string(motorsInfo[iNode].CmdAcc);
		entries->values[5]->setString(temp.c_str());

		temp = std::to_string(motorsInfo[iNode].MeasuredPos);
		entries->values[6]->setString(temp.c_str());

		temp = std::to_string(motorsInfo[iNode].MeasuredVel);
		entries->values[7]->setString(temp.c_str());

		temp = std::to_string(motorsInfo[iNode].MeasuredTrq);
		entries->values[8]->setString(temp.c_str());
	}
	else {
		temp = "Not Available";
		entries->values[1]->setString(temp.c_str());
		entries->values[2]->setString("..");
		entries->values[3]->setString("..");
		entries->values[4]->setString("..");
		entries->values[5]->setString("..");
		entries->values[6]->setString("..");
		entries->values[7]->setString("..");
		entries->values[8]->setString("..");
	}
}

void MotorControllerCHOP::fillDebugInfo(OP_InfoDATEntries* entries)
{
	std::string temp;

	entries->values[0]->setString("..");
	entries->values[1]->setString("..");
	entries->values[2]->setString("..");
	entries->values[3]->setString("..");
	entries->values[4]->setString("..");
	entries->values[5]->setString("..");
	entries->values[6]->setString("..");
	entries->values[7]->setString("..");
	entries->values[8]->setString("..");
}
