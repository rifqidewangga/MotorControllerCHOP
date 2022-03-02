#pragma once

#include "pubSysCls.h"
using namespace sFnd;

#define DEFAULT_ACC_LIM_RPM_PER_SEC 100000
#define DEFAULT_VEL_LIM_RPM         700
#define DEFAULT_TIME_TILL_TIMEOUT   10000

enum Status
{
	SUCCESS = 0,
	PORT_NOT_FOUND = 1,
	TIMEOUT = 2,
	HOMING_TIMEOUT = 3,
	BUSY = 4,
	ERROR_CONTROLLER = 66
};

class SCHubController
{
private:
	bool _status = true;
	SysManager* _myMgr = nullptr;
	const size_t _portID = 0;
	
	// For now limit to only support single port, the lowest port on device manager, this enable up to 16 motors
	int initializePort();

	int homeMotor(INode& theNode);
	void homeMotors();

public:
	SCHubController();
	~SCHubController();

	void	enableMotor(size_t iNode, bool newState);
	bool	enableMotor(size_t iNode);
	
	int		rotateMotor(
				size_t iNode, 
				int32_t distanceCnts, double velLimi=DEFAULT_VEL_LIM_RPM, double accLimit=DEFAULT_ACC_LIM_RPM_PER_SEC);

	double getMeasuredPos(size_t iNode);
	double getMeasuredVel(size_t iNode);
	double getMeasuredTrq(size_t iNode);

	Uint16 getNodeCount();
};

