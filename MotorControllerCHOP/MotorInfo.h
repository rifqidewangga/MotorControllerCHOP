#pragma once

struct MotorInfo
{
	double	CmpPos		= 0.0;
	double	CmdVel		= 0.0;
	double	CmdAcc		= 0.0;

	bool	IsEnable	= false;
	double	MeasuredPos = 0.0;
	double	MeasuredVel = 0.0;
	double	MeasuredTrq = 0.0;
};