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
	updateNodeCount();
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
	updateNodeCount();

	iNode = inputs->getParInt("Inode");
	
	isEnable = inputs->getParInt("Enable");
	
	if (isNodeAvailable(iNode))
	{
#ifndef SIMULATION
		bool isNodeEnabled = motorController.enableMotor(iNode);
#else
		bool isNodeEnabled = isEnable;
#endif // !SIMULATION

		if (isEnable != isNodeEnabled)
		{
#ifndef SIMULATION
			motorController.enableMotor(iNode, isEnable);
#endif // !SIMULATION
		}
	}

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
	infoSize->rows = 11;
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
		fillNodeHeader(entries);

	if (index > 0 && index < 10)
		fillNodeInfo(entries, index - 1);

	if (index == 10)
		fillDebugInfo(entries);
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
		if (isNodeAvailable(iNode))
			nRotateClicked++;
#ifndef SIMULATION
			motorController.rotateMotor(iNode, counts, velocity, acceleration);
#endif // !SIMULATION
	}
}

void MotorControllerCHOP::updateNodeCount()
{
#ifndef SIMULATION
	nodeCount = motorController.getNodeCount();
#else
	nodeCount = 2;
#endif // !SIMULATION
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
	entries->values[3]->setString("position (cnts)");
	entries->values[4]->setString("velocity (rpm)");
	entries->values[5]->setString("torque (..)");
	entries->values[6]->setString("reserved");
	entries->values[7]->setString("reserved");
}

void MotorControllerCHOP::fillNodeInfo(OP_InfoDATEntries* entries, int iNode)
{
	std::string temp;

	temp = std::to_string(iNode);
	entries->values[0]->setString(temp.c_str());

	if (isNodeAvailable(iNode))
		temp = "Available";
	else
		temp = "Not Available";
	entries->values[1]->setString(temp.c_str());
	
	entries->values[2]->setString("..");
	entries->values[3]->setString("..");
	entries->values[4]->setString("..");
	entries->values[5]->setString("..");
	entries->values[6]->setString("..");
	entries->values[7]->setString("..");
}

void MotorControllerCHOP::fillDebugInfo(OP_InfoDATEntries* entries)
{
	std::string temp;

	entries->values[0]->setString("debugoutput");

	temp = std::to_string(nRotateClicked);
	entries->values[1]->setString(temp.c_str());
	
	entries->values[2]->setString("..");
	entries->values[3]->setString("..");
	entries->values[4]->setString("..");
	entries->values[5]->setString("..");
	entries->values[6]->setString("..");
	entries->values[7]->setString("..");
}
