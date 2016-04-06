/********************************************
 *  pmacController.cpp
 * 
 *  PMAC Asyn motor based on the 
 *  asynMotorController class.
 * 
 *  Matthew Pearson
 *  23 May 2012
 * 
 ********************************************/



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include <iostream>
using std::cout;
using std::endl;
using std::dec;

#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <iocsh.h>
#include <drvSup.h>
#include <registryFunction.h>

#include "asynOctetSyncIO.h"

#include "pmacController.h"
#include "pmacCSController.h"
#include "pmacCSMonitor.h"

#include <epicsExport.h>

static const char *driverName = "pmacController";

const epicsUInt32 pmacController::PMAC_MAXBUF_ = PMAC_MAXBUF;
const epicsFloat64 pmacController::PMAC_TIMEOUT_ = 5.0;
const epicsUInt32 pmacController::PMAC_FEEDRATE_LIM_ = 100;
const epicsUInt32 pmacController::PMAC_ERROR_PRINT_TIME_ = 600; //seconds
const epicsUInt32 pmacController::PMAC_FORCED_FAST_POLLS_ = 10;
const epicsUInt32 pmacController::PMAC_OK_ = 0;
const epicsUInt32 pmacController::PMAC_ERROR_ = 1;
const epicsUInt32 pmacController::PMAC_FEEDRATE_DEADBAND_ = 1;
const epicsInt32 pmacController::PMAC_CID_PMAC_ = 602413;
const epicsInt32 pmacController::PMAC_CID_GEOBRICK_ = 603382;

const epicsUInt32 pmacController::PMAC_STATUS1_MAXRAPID_SPEED    = (0x1<<0);
const epicsUInt32 pmacController::PMAC_STATUS1_ALT_CMNDOUT_MODE  = (0x1<<1);
const epicsUInt32 pmacController::PMAC_STATUS1_SOFT_POS_CAPTURE  = (0x1<<2);
const epicsUInt32 pmacController::PMAC_STATUS1_ERROR_TRIGGER     = (0x1<<3);
const epicsUInt32 pmacController::PMAC_STATUS1_FOLLOW_ENABLE     = (0x1<<4);
const epicsUInt32 pmacController::PMAC_STATUS1_FOLLOW_OFFSET     = (0x1<<5);
const epicsUInt32 pmacController::PMAC_STATUS1_PHASED_MOTOR      = (0x1<<6);
const epicsUInt32 pmacController::PMAC_STATUS1_ALT_SRC_DEST      = (0x1<<7);
const epicsUInt32 pmacController::PMAC_STATUS1_USER_SERVO        = (0x1<<8);
const epicsUInt32 pmacController::PMAC_STATUS1_USER_PHASE        = (0x1<<9);
const epicsUInt32 pmacController::PMAC_STATUS1_HOMING            = (0x1<<10);
const epicsUInt32 pmacController::PMAC_STATUS1_BLOCK_REQUEST     = (0x1<<11);
const epicsUInt32 pmacController::PMAC_STATUS1_DECEL_ABORT       = (0x1<<12);
const epicsUInt32 pmacController::PMAC_STATUS1_DESIRED_VELOCITY_ZERO = (0x1<<13);
const epicsUInt32 pmacController::PMAC_STATUS1_DATABLKERR        = (0x1<<14);
const epicsUInt32 pmacController::PMAC_STATUS1_DWELL             = (0x1<<15);
const epicsUInt32 pmacController::PMAC_STATUS1_INTEGRATE_MODE    = (0x1<<16);
const epicsUInt32 pmacController::PMAC_STATUS1_MOVE_TIME_ON      = (0x1<<17);
const epicsUInt32 pmacController::PMAC_STATUS1_OPEN_LOOP         = (0x1<<18);
const epicsUInt32 pmacController::PMAC_STATUS1_AMP_ENABLED       = (0x1<<19);
const epicsUInt32 pmacController::PMAC_STATUS1_X_SERVO_ON        = (0x1<<20);
const epicsUInt32 pmacController::PMAC_STATUS1_POS_LIMIT_SET     = (0x1<<21);
const epicsUInt32 pmacController::PMAC_STATUS1_NEG_LIMIT_SET     = (0x1<<22);
const epicsUInt32 pmacController::PMAC_STATUS1_MOTOR_ON          = (0x1<<23);

const epicsUInt32 pmacController::PMAC_STATUS2_IN_POSITION       = (0x1<<0);
const epicsUInt32 pmacController::PMAC_STATUS2_WARN_FOLLOW_ERR   = (0x1<<1);
const epicsUInt32 pmacController::PMAC_STATUS2_ERR_FOLLOW_ERR    = (0x1<<2);
const epicsUInt32 pmacController::PMAC_STATUS2_AMP_FAULT         = (0x1<<3);
const epicsUInt32 pmacController::PMAC_STATUS2_NEG_BACKLASH      = (0x1<<4);
const epicsUInt32 pmacController::PMAC_STATUS2_I2T_AMP_FAULT     = (0x1<<5);
const epicsUInt32 pmacController::PMAC_STATUS2_I2_FOLLOW_ERR     = (0x1<<6);
const epicsUInt32 pmacController::PMAC_STATUS2_TRIGGER_MOVE      = (0x1<<7);
const epicsUInt32 pmacController::PMAC_STATUS2_PHASE_REF_ERR     = (0x1<<8);
const epicsUInt32 pmacController::PMAC_STATUS2_PHASE_SEARCH      = (0x1<<9);
const epicsUInt32 pmacController::PMAC_STATUS2_HOME_COMPLETE     = (0x1<<10);
const epicsUInt32 pmacController::PMAC_STATUS2_POS_LIMIT_STOP    = (0x1<<11);
const epicsUInt32 pmacController::PMAC_STATUS2_DESIRED_STOP      = (0x1<<12);
const epicsUInt32 pmacController::PMAC_STATUS2_FORE_IN_POS       = (0x1<<13);
const epicsUInt32 pmacController::PMAC_STATUS2_NA14              = (0x1<<14);
const epicsUInt32 pmacController::PMAC_STATUS2_ASSIGNED_CS       = (0x1<<15);

/*Global status ???*/
const epicsUInt32 pmacController::PMAC_GSTATUS_CARD_ADDR             = (0x1<<0);
const epicsUInt32 pmacController::PMAC_GSTATUS_ALL_CARD_ADDR         = (0x1<<1);
const epicsUInt32 pmacController::PMAC_GSTATUS_RESERVED              = (0x1<<2);
const epicsUInt32 pmacController::PMAC_GSTATUS_PHASE_CLK_MISS        = (0x1<<3);
const epicsUInt32 pmacController::PMAC_GSTATUS_MACRO_RING_ERRORCHECK = (0x1<<4);
const epicsUInt32 pmacController::PMAC_GSTATUS_MACRO_RING_COMMS      = (0x1<<5);
const epicsUInt32 pmacController::PMAC_GSTATUS_TWS_PARITY_ERROR      = (0x1<<6);
const epicsUInt32 pmacController::PMAC_GSTATUS_CONFIG_ERROR          = (0x1<<7);
const epicsUInt32 pmacController::PMAC_GSTATUS_ILLEGAL_LVAR          = (0x1<<8);
const epicsUInt32 pmacController::PMAC_GSTATUS_REALTIME_INTR         = (0x1<<9);
const epicsUInt32 pmacController::PMAC_GSTATUS_FLASH_ERROR           = (0x1<<10);
const epicsUInt32 pmacController::PMAC_GSTATUS_DPRAM_ERROR           = (0x1<<11);
const epicsUInt32 pmacController::PMAC_GSTATUS_CKSUM_ACTIVE          = (0x1<<12);
const epicsUInt32 pmacController::PMAC_GSTATUS_CKSUM_ERROR           = (0x1<<13);
const epicsUInt32 pmacController::PMAC_GSTATUS_LEADSCREW_COMP        = (0x1<<14);
const epicsUInt32 pmacController::PMAC_GSTATUS_WATCHDOG              = (0x1<<15);
const epicsUInt32 pmacController::PMAC_GSTATUS_SERVO_REQ             = (0x1<<16);
const epicsUInt32 pmacController::PMAC_GSTATUS_DATA_GATHER_START     = (0x1<<17);
const epicsUInt32 pmacController::PMAC_GSTATUS_RESERVED2             = (0x1<<18);
const epicsUInt32 pmacController::PMAC_GSTATUS_DATA_GATHER_ON        = (0x1<<19);
const epicsUInt32 pmacController::PMAC_GSTATUS_SERVO_ERROR           = (0x1<<20);
const epicsUInt32 pmacController::PMAC_GSTATUS_CPUTYPE               = (0x1<<21);
const epicsUInt32 pmacController::PMAC_GSTATUS_REALTIME_INTR_RE      = (0x1<<22);
const epicsUInt32 pmacController::PMAC_GSTATUS_RESERVED3             = (0x1<<23);

const epicsUInt32 pmacController::PMAC_HARDWARE_PROB = (PMAC_GSTATUS_MACRO_RING_ERRORCHECK | PMAC_GSTATUS_MACRO_RING_COMMS | PMAC_GSTATUS_REALTIME_INTR | PMAC_GSTATUS_FLASH_ERROR | PMAC_GSTATUS_DPRAM_ERROR | PMAC_GSTATUS_CKSUM_ERROR | PMAC_GSTATUS_WATCHDOG | PMAC_GSTATUS_SERVO_ERROR);

const epicsUInt32 pmacController::PMAX_AXIS_GENERAL_PROB1 = 0;
const epicsUInt32 pmacController::PMAX_AXIS_GENERAL_PROB2 = (PMAC_STATUS2_DESIRED_STOP | PMAC_STATUS2_AMP_FAULT);

const char *pmacController::PMAC_C_ForwardKinematicString[] = {
    "PMAC_C_FWD_KIN_1",
    "PMAC_C_FWD_KIN_2",
    "PMAC_C_FWD_KIN_3",
    "PMAC_C_FWD_KIN_4",
    "PMAC_C_FWD_KIN_5",
    "PMAC_C_FWD_KIN_6",
    "PMAC_C_FWD_KIN_7",
    "PMAC_C_FWD_KIN_8",
    "PMAC_C_FWD_KIN_9",
    "PMAC_C_FWD_KIN_10",
    "PMAC_C_FWD_KIN_11",
    "PMAC_C_FWD_KIN_12",
    "PMAC_C_FWD_KIN_13",
    "PMAC_C_FWD_KIN_14",
    "PMAC_C_FWD_KIN_15",
    "PMAC_C_FWD_KIN_16"
};

const char *pmacController::PMAC_C_InverseKinematicString[] = {
    "PMAC_C_INV_KIN_1",
    "PMAC_C_INV_KIN_2",
    "PMAC_C_INV_KIN_3",
    "PMAC_C_INV_KIN_4",
    "PMAC_C_INV_KIN_5",
    "PMAC_C_INV_KIN_6",
    "PMAC_C_INV_KIN_7",
    "PMAC_C_INV_KIN_8",
    "PMAC_C_INV_KIN_9",
    "PMAC_C_INV_KIN_10",
    "PMAC_C_INV_KIN_11",
    "PMAC_C_INV_KIN_12",
    "PMAC_C_INV_KIN_13",
    "PMAC_C_INV_KIN_14",
    "PMAC_C_INV_KIN_15",
    "PMAC_C_INV_KIN_16"
};

//C function prototypes, for the functions that can be called on IOC shell.
//Some of these functions are provided to ease transition to the model 3 driver. Some of these
//functions could be handled by the parameter library.
extern "C"
{
  asynStatus pmacCreateController(const char *portName, const char *lowLevelPortName, int lowLevelPortAddress, 
					 int numAxes, int movingPollPeriod, int idlePollPeriod);
  
  asynStatus pmacCreateAxis(const char *pmacName, int axis);

  asynStatus pmacCreateAxes(const char *pmacName, int numAxes);
  
  asynStatus pmacDisableLimitsCheck(const char *controller, int axis, int allAxes);
  
  asynStatus pmacSetAxisScale(const char *controller, int axis, int scale);
  
  asynStatus pmacSetOpenLoopEncoderAxis(const char *controller, int axis, int encoder_axis);

}

static void trajTaskC(void *drvPvt)
{
  pmacController *pPvt = (pmacController *)drvPvt;
  pPvt->trajectoryTask();
}

/**
 * pmacController constructor.
 * @param portName The Asyn port name to use (that the motor record connects to).
 * @param lowLevelPortName The name of the low level port that has already been created, to enable comms to the controller.
 * @param lowLevelPortAddress The asyn address for the low level port
 * @param numAxes The number of axes on the controller (1 based)
 * @param movingPollPeriod The time (in milliseconds) between polling when axes are moving
 * @param movingPollPeriod The time (in milliseconds) between polling when axes are idle
 */
pmacController::pmacController(const char *portName, const char *lowLevelPortName, int lowLevelPortAddress, 
			       int numAxes, double movingPollPeriod, double idlePollPeriod)
  : asynMotorController(portName, numAxes+1, NUM_MOTOR_DRIVER_PARAMS+NUM_PMAC_PARAMS,
			0, // No additional interfaces
			0, // No addition interrupt interfaces
			ASYN_CANBLOCK | ASYN_MULTIDEVICE, 
			1, // autoconnect
			0, 0),  // Default priority and stack size
	pmacDebugger("pmacController")
{
  int index = 0;
  static const char *functionName = "pmacController::pmacController";

  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s Constructor.\n", functionName);

  //Initialize non static data members
  parameterIndex_ = 0;
  lowLevelPortUser_ = NULL;
  movesDeferred_ = 0;
  nowTimeSecs_ = 0.0;
  lastTimeSecs_ = 0.0;
  epicsTimeGetCurrent(&lastMediumTime_);
  epicsTimeAddSeconds(&lastMediumTime_, PMAC_MEDIUM_LOOP_TIME / -1000.0);
  epicsTimeGetCurrent(&lastSlowTime_);
  epicsTimeAddSeconds(&lastSlowTime_, PMAC_SLOW_LOOP_TIME / -1000.0);
  printNextError_ = false;
  feedRatePoll_ = false;
  movingPollPeriod_ = movingPollPeriod;
  idlePollPeriod_ = idlePollPeriod;
  profileInitialized_ = false;
  tScanExecuting_ = 0;
  tScanCSNo_ = 0;
  tScanAxisMask_ = 0;
  tScanPointCtr_ = 0;
  tScanPmacBufferPtr_ = 0;
  tScanPmacTotalPts_ = 0;
  tScanPmacStatus_ = 0;
  tScanPmacBufferNumber_ = 0;
  tScanPmacBufferAddressA_ = 0;
  tScanPmacBufferAddressB_ = 0;
  tScanPmacBufferSize_ = 32;      // TODO: This must be read from the PMAC
  tScanPositions_ = NULL;

  // Create the parameter hashtables
  pIntParams_ = new IntegerHashtable();
  pDoubleParams_ = new IntegerHashtable();
  pStringParams_ = new IntegerHashtable();
  pWriteParams_ = new StringHashtable();

  pAxes_ = (pmacAxis **)(asynMotorController::pAxes_);

  // Initialise the table of CS controller pointers
  pCSControllers_ = (pmacCSController **)malloc(PMAC_MAX_CS * sizeof(pmacCSController *));
  for (index = 0; index < PMAC_MAX_CS; index++){
    pCSControllers_[index] = NULL;
  }

  //Create dummy axis for asyn address 0. This is used for controller parameters.
  //pAxisZero = new pmacAxis(this, 0);
  pAxisZero = new pmacCSMonitor(this);
  pGroupList = new pmacCsGroups(this);

  //Create controller-specific parameters
  createParam(PMAC_C_FirstParamString,        asynParamInt32,   &PMAC_C_FirstParam_);
  createParam(PMAC_C_GlobalStatusString,      asynParamInt32,   &PMAC_C_GlobalStatus_);
  createParam(PMAC_C_CommsErrorString,        asynParamInt32,   &PMAC_C_CommsError_);
  createParam(PMAC_C_FeedRateString,          asynParamInt32,   &PMAC_C_FeedRate_);
  createParam(PMAC_C_FeedRateLimitString,     asynParamInt32,   &PMAC_C_FeedRateLimit_);
  createParam(PMAC_C_FeedRatePollString,      asynParamInt32,   &PMAC_C_FeedRatePoll_);
  createParam(PMAC_C_FeedRateProblemString,   asynParamInt32,   &PMAC_C_FeedRateProblem_);
  createParam(PMAC_C_CoordSysGroup,           asynParamInt32,   &PMAC_C_CoordSysGroup_);
  createParam(PMAC_C_FastUpdateTimeString,    asynParamFloat64, &PMAC_C_FastUpdateTime_);
  createParam(PMAC_C_LastParamString,         asynParamInt32,   &PMAC_C_LastParam_);
  createParam(PMAC_C_AxisCSString,            asynParamInt32,   &PMAC_C_AxisCS_);
  createParam(PMAC_C_WriteCmdString,          asynParamOctet,   &PMAC_C_WriteCmd_);
  createParam(PMAC_C_KillAxisString,          asynParamInt32,   &PMAC_C_KillAxis_);
  createParam(PMAC_C_TrajBufferLengthString,  asynParamInt32,   &PMAC_C_TrajBufferLength_);
  createParam(PMAC_C_TrajTotalPointsString,   asynParamInt32,   &PMAC_C_TrajTotalPoints_);
  createParam(PMAC_C_TrajStatusString,        asynParamInt32,   &PMAC_C_TrajStatus_);
  createParam(PMAC_C_TrajCurrentIndexString,  asynParamInt32,   &PMAC_C_TrajCurrentIndex_);
  createParam(PMAC_C_TrajCurrentBufferString, asynParamInt32,   &PMAC_C_TrajCurrentBuffer_);
  createParam(PMAC_C_TrajBuffAdrAString,      asynParamInt32,   &PMAC_C_TrajBuffAdrA_);
  createParam(PMAC_C_TrajBuffAdrBString,      asynParamInt32,   &PMAC_C_TrajBuffAdrB_);
  createParam(PMAC_C_TrajBuffFillAString,     asynParamInt32,   &PMAC_C_TrajBuffFillA_);
  createParam(PMAC_C_TrajBuffFillBString,     asynParamInt32,   &PMAC_C_TrajBuffFillB_);
  createParam(PMAC_C_TrajRunTimeString,       asynParamFloat64, &PMAC_C_TrajRunTime_);
  createParam(PMAC_C_TrajCSNumberString,      asynParamInt32,   &PMAC_C_TrajCSNumber_);
  createParam(PMAC_C_TrajPercentString,       asynParamFloat64, &PMAC_C_TrajPercent_);
  createParam(PMAC_C_TrajEStatusString,       asynParamInt32,   &PMAC_C_TrajEStatus_);
  createParam(PMAC_C_NoOfMsgsString,          asynParamInt32,   &PMAC_C_NoOfMsgs_);
  createParam(PMAC_C_TotalBytesWrittenString, asynParamInt32,   &PMAC_C_TotalBytesWritten_);
  createParam(PMAC_C_TotalBytesReadString,    asynParamInt32,   &PMAC_C_TotalBytesRead_);
  createParam(PMAC_C_MsgBytesWrittenString,   asynParamInt32,   &PMAC_C_MsgBytesWritten_);
  createParam(PMAC_C_MsgBytesReadString,      asynParamInt32,   &PMAC_C_MsgBytesRead_);
  createParam(PMAC_C_MsgTimeString,           asynParamInt32,   &PMAC_C_MsgTime_);
  createParam(PMAC_C_MaxBytesWrittenString,   asynParamInt32,   &PMAC_C_MaxBytesWritten_);
  createParam(PMAC_C_MaxBytesReadString,      asynParamInt32,   &PMAC_C_MaxBytesRead_);
  createParam(PMAC_C_MaxTimeString,           asynParamInt32,   &PMAC_C_MaxTime_);
  createParam(PMAC_C_AveBytesWrittenString,   asynParamInt32,   &PMAC_C_AveBytesWritten_);
  createParam(PMAC_C_AveBytesReadString,      asynParamInt32,   &PMAC_C_AveBytesRead_);
  createParam(PMAC_C_AveTimeString,           asynParamInt32,   &PMAC_C_AveTime_);
  for (index = 0; index < PMAC_MAX_CS; index++){
    createParam(PMAC_C_ForwardKinematicString[index], asynParamOctet, &PMAC_C_ForwardKinematic_[index]);
    createParam(PMAC_C_InverseKinematicString[index], asynParamOctet, &PMAC_C_InverseKinematic_[index]);
  }

  pBroker_ = new pmacMessageBroker(this->pasynUserSelf);

  //printf("** Broker created\n");
  //printf("** Variables added\n");

  //Connect our Asyn user to the low level port that is a parameter to this constructor
//  if (lowLevelPortConnect(lowLevelPortName, lowLevelPortAddress, &lowLevelPortUser_,
//		  (char*)"\006", (char*)"\r") != asynSuccess) {

  if (pBroker_->connect(lowLevelPortName, lowLevelPortAddress) != asynSuccess){
    printf("%s: Failed to connect to low level asynOctetSyncIO port %s\n", functionName, lowLevelPortName);
    setIntegerParam(PMAC_C_CommsError_, PMAC_ERROR_);
  } else {
    setIntegerParam(PMAC_C_CommsError_, PMAC_OK_);
  }


  // Read the kinematics
  this->storeKinematics();
//  this->listPLCProgram(11, buff, 20000);
//  printf("PLC11: %s\n", buff);
//  this->listPLCProgram(17, buff, 20000);
//  printf("PLC17: %s\n", buff);
//  this->listPLCProgram(18, buff, 20000);
//  printf("PLC18: %s\n", buff);

  bool paramStatus = true;
  paramStatus = ((setIntegerParam(PMAC_C_GlobalStatus_, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(PMAC_C_FeedRateProblem_, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(PMAC_C_FeedRateLimit_, 100) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(PMAC_C_FastUpdateTime_, 0.0) == asynSuccess) && paramStatus);
  // Initialise the trajectory interface
  paramStatus = ((setIntegerParam(profileBuildState_, PROFILE_BUILD_DONE) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(profileExecuteState_, PROFILE_EXECUTE_DONE) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(PMAC_C_TrajRunTime_, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(PMAC_C_TrajCSNumber_, tScanCSNo_) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(PMAC_C_TrajEStatus_, 0) == asynSuccess) && paramStatus);
  // Initialise the statistics
  paramStatus = ((setIntegerParam(PMAC_C_NoOfMsgs_, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(PMAC_C_TotalBytesWritten_, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(PMAC_C_TotalBytesRead_, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(PMAC_C_MsgBytesWritten_, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(PMAC_C_MsgBytesRead_, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(PMAC_C_MsgTime_, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(PMAC_C_MaxBytesWritten_, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(PMAC_C_MaxBytesRead_, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(PMAC_C_MaxTime_, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(PMAC_C_AveBytesWritten_, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(PMAC_C_AveBytesRead_, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(PMAC_C_AveTime_, 0) == asynSuccess) && paramStatus);

  callParamCallbacks();

  if (!paramStatus) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s Unable To Set Driver Parameters In Constructor.\n", functionName);
  }
 
  // Add the items required for global status
  pBroker_->addReadVariable(pmacMessageBroker::PMAC_FAST_READ, "???");
  pBroker_->addReadVariable(pmacMessageBroker::PMAC_FAST_READ, "%");

  // Add the PMAC P variables required for trajectory scanning
  // Fast readout required of these values
  pBroker_->addReadVariable(pmacMessageBroker::PMAC_FAST_READ, PMAC_TRAJ_STATUS);
  pBroker_->addReadVariable(pmacMessageBroker::PMAC_FAST_READ, PMAC_TRAJ_CURRENT_INDEX);
  pBroker_->addReadVariable(pmacMessageBroker::PMAC_FAST_READ, PMAC_TRAJ_CURRENT_BUFFER);
  pBroker_->addReadVariable(pmacMessageBroker::PMAC_FAST_READ, PMAC_TRAJ_TOTAL_POINTS);
  // Slow readout required of these values
  pBroker_->addReadVariable(pmacMessageBroker::PMAC_SLOW_READ, PMAC_TRAJ_BUFFER_LENGTH);
  pBroker_->addReadVariable(pmacMessageBroker::PMAC_SLOW_READ, PMAC_TRAJ_BUFF_ADR_A);
  pBroker_->addReadVariable(pmacMessageBroker::PMAC_SLOW_READ, PMAC_TRAJ_BUFF_ADR_B);


  // Register this class for updates
  pBroker_->registerForUpdates(this, pmacMessageBroker::PMAC_FAST_READ);
  pBroker_->registerForUpdates(this, pmacMessageBroker::PMAC_MEDIUM_READ);
  pBroker_->registerForUpdates(this, pmacMessageBroker::PMAC_SLOW_READ);

  // Create the epicsEvents for signaling to start and stop scanning
  this->startEventId_ = epicsEventCreate(epicsEventEmpty);
  if (!this->startEventId_){
    printf("%s:%s epicsEventCreate failure for start event\n", driverName, functionName);
  }
  this->stopEventId_ = epicsEventCreate(epicsEventEmpty);
  if (!this->stopEventId_){
    printf("%s:%s epicsEventCreate failure for stop event\n", driverName, functionName);
  }

  // Create the thread that executes trajectory scans
  epicsThreadCreate("TrajScanTask",
                    epicsThreadPriorityMedium,
                    epicsThreadGetStackSize(epicsThreadStackMedium),
                    (EPICSTHREADFUNC)trajTaskC,
                    this);

}


pmacController::~pmacController(void) 
{
  //Destructor. Should never get here.
  delete pAxisZero;
}

void pmacController::startPMACPolling()
{
  // Start the underlying polling thread (asynMotorController)
  startPoller(movingPollPeriod_, idlePollPeriod_, PMAC_FORCED_FAST_POLLS_);
}

void pmacController::setDebugLevel(int level, int axis)
{
  // Check if an axis or controller wide debug is to be set
  if (axis == 0){
    printf("Setting PMAC controller debug level to %d\n", level);
    // Set the level for the controller
    this->setLevel(level);
    // Set the level for the broker
    pBroker_->setLevel(level);
  } else {
    if (this->getAxis(axis) != NULL){
      printf("Setting PMAC axis %d debug level to %d\n", axis, level);
      this->getAxis(axis)->setLevel(level);
    }
  }
}

asynStatus pmacController::drvUserCreate(asynUser *pasynUser, const char *drvInfo, const char **pptypeName, size_t *psize)
{
  static const char *functionName = "drvUserCreate";
  asynStatus status = asynSuccess;
  int index;
  // Accepted parameter formats
  //
  // For reading variables
  // PMAC_VxF_...  => PMAC Variable Fast Loop
  // PMAC_VxM_...  => PMAC Variable Medium Loop
  // PMAC_VxS_...  => PMAC Variable Slow Loop
  //
  // x is I for int, D for double or S for string
  //
  // The must be no j or = in a variable, these items will simply be polled for their current status
  //
  // For commands
  // PMAC_CMDx_... => Send command to PMAC
  //
  // x is I for int, D for double or S for string

  // For Writing only
  // PMAC_WI_... => Write Integer Value
  // PMAC_WD_... => Write Double Value
  // PMAC_WS_... => Write String Value
  //
  // Writing to these parameters will result in immediate writes to the PMAC


  // Check if we have already provided maximum number of custom parameters
  if (parameterIndex_ > PMAC_MAX_PARAMETERS){
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
          "%s:%s: Not enough space allocated to store all camera features, increase NFEATURES\n",
          driverName, functionName);
    status = asynError;
  }

  if (status == asynSuccess){

    if (findParam(drvInfo, &index) && strlen(drvInfo) > 9 && strncmp(drvInfo, "PMAC_V", 6) == 0 && drvInfo[8] == '_'){

      // Retrieve the name of the variable
      char *pmacVariable = epicsStrDup(drvInfo + 9);

      printf("Creating new parameter %s\n", pmacVariable);
      // Check for I, D or S in drvInfo[7]
      switch(drvInfo[6]) {
        case 'I':
          // Create the parameter
          createParam(drvInfo, asynParamInt32, &(this->parameters[parameterIndex_]));
          setIntegerParam(this->parameters[parameterIndex_], 0);
          // Add variable to integer parameter hashtable
          this->pIntParams_->insert(pmacVariable, this->parameters[parameterIndex_]);
          parameterIndex_++;
          break;
        case 'D':
          createParam(drvInfo, asynParamFloat64, &(this->parameters[parameterIndex_]));
          // Add variable to double parameter hashtable
          this->pDoubleParams_->insert(pmacVariable, this->parameters[parameterIndex_]);
          parameterIndex_++;
          break;
        case 'S':
          createParam(drvInfo, asynParamOctet, &(this->parameters[parameterIndex_]));
          // Add variable to string parameter hashtable
          this->pStringParams_->insert(pmacVariable, this->parameters[parameterIndex_]);
          parameterIndex_++;
          break;
        default:
          asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: Expected PMAC_Vtx_... where x is one of I, D or S. Got '%c'\n",
                    driverName, functionName, drvInfo[6]);
          status = asynError;
      }

      if (status == asynSuccess){
        // Check for F, M or S in drvInfo[6]
        switch(drvInfo[7]) {
          case 'F':
            this->pBroker_->addReadVariable(pmacMessageBroker::PMAC_FAST_READ, pmacVariable);
            break;
          case 'M':
            this->pBroker_->addReadVariable(pmacMessageBroker::PMAC_MEDIUM_READ, pmacVariable);
            break;
          case 'S':
            this->pBroker_->addReadVariable(pmacMessageBroker::PMAC_SLOW_READ, pmacVariable);
            break;
          default:
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s:%s: Expected PMAC_Vtx_... where t is one of F, M or S. Got '%c'\n",
                      driverName, functionName, drvInfo[7]);
            status = asynError;
        }

        if (status == asynSuccess){
          this->pWriteParams_->insert(drvInfo, pmacVariable);
        }
      }



      // Create the parameter, store the param ID along with the var name in the hashtable

    }

    if (findParam(drvInfo, &index) && strlen(drvInfo) > 9 && strncmp(drvInfo, "PMAC_W", 6) == 0 && drvInfo[7] == '_'){

      // Retrieve the name of the variable
      char *pmacVariable = epicsStrDup(drvInfo + 8);

      printf("Creating new write only parameter %s\n", pmacVariable);
      // Check for I, D or S in drvInfo[7]
      switch(drvInfo[6]) {
        case 'I':
          // Create the parameter
          createParam(drvInfo, asynParamInt32, &(this->parameters[parameterIndex_]));
          // Add variable to write only parameter hashtable
          this->pWriteParams_->insert(drvInfo, pmacVariable);
          parameterIndex_++;
          break;
        case 'D':
          createParam(drvInfo, asynParamFloat64, &(this->parameters[parameterIndex_]));
          // Add variable to double parameter hashtable
          this->pWriteParams_->insert(drvInfo, pmacVariable);
          parameterIndex_++;
          break;
        case 'S':
          createParam(drvInfo, asynParamOctet, &(this->parameters[parameterIndex_]));
          // Add variable to string parameter hashtable
          this->pWriteParams_->insert(drvInfo, pmacVariable);
          parameterIndex_++;
          break;
        default:
          asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: Expected PMAC_Wx_... where x is one of I, D or S. Got '%c'\n",
                    driverName, functionName, drvInfo[6]);
          status = asynError;
      }
    }

  }

  if (status == asynSuccess){
    // Now return baseclass result
    status = asynMotorController::drvUserCreate(pasynUser, drvInfo, pptypeName, psize);
  }
  return status;
}


void pmacController::callback(pmacCommandStore *sPtr, int type)
{
  static const char *functionName = "callback";
  debug(DEBUG_FLOW, functionName);

  if (type == pmacMessageBroker::PMAC_FAST_READ){
    // Parse PMAC global status
    this->newGetGlobalStatus(sPtr);
  }

  // If this is a slow callback then execute the slow update
  if (type == pmacMessageBroker::PMAC_SLOW_READ){
    // Parse PMAC global status
    this->slowUpdate(sPtr);
  }

  // Loop over parameter list and search for values

  // Check for integer params
  std::string key = this->pIntParams_->firstKey();
  if (key != ""){
    if (sPtr->checkForItem(key)){
      int val = 0;
      sscanf(sPtr->readValue(key).c_str(), "%d", &val);
      debug(DEBUG_VARIABLE, functionName, "Found key  ", key.c_str());
      debug(DEBUG_VARIABLE, functionName, "      value", val);
      setIntegerParam(this->pIntParams_->lookup(key), val);
    }
    while (this->pIntParams_->hasNextKey()){
      key = this->pIntParams_->nextKey();
      if (sPtr->checkForItem(key)){
        int val = 0;
        sscanf(sPtr->readValue(key).c_str(), "%d", &val);
        debug(DEBUG_VARIABLE, functionName, "Found key  ", key.c_str());
        debug(DEBUG_VARIABLE, functionName, "      value", val);
        setIntegerParam(this->pIntParams_->lookup(key), val);
      }
    }
    callParamCallbacks();
  }

  // Check for double params
  key = this->pDoubleParams_->firstKey();
  if (key != ""){
    if (sPtr->checkForItem(key)){
      double val = 0;
      sscanf(sPtr->readValue(key).c_str(), "%lf", &val);
      debug(DEBUG_VARIABLE, functionName, "Found key  ", key.c_str());
      debug(DEBUG_VARIABLE, functionName, "      value", val);
      setDoubleParam(this->pDoubleParams_->lookup(key), val);
    }
    while (this->pDoubleParams_->hasNextKey()){
      key = this->pDoubleParams_->nextKey();
      if (sPtr->checkForItem(key)){
        double val = 0;
        sscanf(sPtr->readValue(key).c_str(), "%lf", &val);
        debug(DEBUG_VARIABLE, functionName, "Found key  ", key.c_str());
        debug(DEBUG_VARIABLE, functionName, "      value", val);
        setDoubleParam(this->pDoubleParams_->lookup(key), val);
      }
    }
    callParamCallbacks();
  }

  // Check for string params
  key = this->pStringParams_->firstKey();
  if (key != ""){
    if (sPtr->checkForItem(key)){
      char val[MAX_STRING_SIZE];
      sscanf(sPtr->readValue(key).c_str(), "%s", val);
      debug(DEBUG_VARIABLE, functionName, "Found key  ", key.c_str());
      debug(DEBUG_VARIABLE, functionName, "      value", (char *)val);
      setStringParam(this->pStringParams_->lookup(key), val);
    }
    while (this->pStringParams_->hasNextKey()){
      key = this->pStringParams_->nextKey();
      if (sPtr->checkForItem(key)){
        char val[MAX_STRING_SIZE];
        sscanf(sPtr->readValue(key).c_str(), "%s", val);
        debug(DEBUG_VARIABLE, functionName, "Found key  ", key.c_str());
        debug(DEBUG_VARIABLE, functionName, "      value", (char *)val);
        setStringParam(this->pDoubleParams_->lookup(key), val);
      }
    }
    callParamCallbacks();
  }

}

asynStatus pmacController::slowUpdate(pmacCommandStore *sPtr)
{
  asynStatus status = asynSuccess;
  int nvals = 0;
  std::string trajPtr = "";
  static const char *functionName = "slowUpdate";
  debug(DEBUG_FLOW, functionName);

  // Read the length of avaialable trajectory buffers
  trajPtr = sPtr->readValue(PMAC_TRAJ_BUFFER_LENGTH);
  if (trajPtr == ""){
    debug(DEBUG_ERROR, functionName, "Problem reading trajectory buffer length", PMAC_TRAJ_BUFFER_LENGTH);
    status = asynError;
  } else {
    nvals = sscanf(trajPtr.c_str(), "%d", &tScanPmacBufferSize_);
    if (nvals != 1) {
      debug(DEBUG_ERROR, functionName, "Error reading trajectory buffer length", PMAC_TRAJ_BUFFER_LENGTH);
      debug(DEBUG_ERROR, functionName, "    nvals", nvals);
      debug(DEBUG_ERROR, functionName, "    response", trajPtr);
      status = asynError;
    } else {
      // Save the value into the parameter
      setIntegerParam(PMAC_C_TrajBufferLength_, tScanPmacBufferSize_);
      debugf(DEBUG_VARIABLE, functionName, "Slow read trajectory buffer length [%s] => %d", PMAC_TRAJ_BUFFER_LENGTH, tScanPmacBufferSize_);
    }
  }

  // Read the address of the A half buffer
  trajPtr = sPtr->readValue(PMAC_TRAJ_BUFF_ADR_A);
  if (trajPtr == ""){
    debug(DEBUG_ERROR, functionName, "Problem reading address of A buffer", PMAC_TRAJ_BUFF_ADR_A);
    status = asynError;
  } else {
    nvals = sscanf(trajPtr.c_str(), "%d", &tScanPmacBufferAddressA_);
    if (nvals != 1) {
      debug(DEBUG_ERROR, functionName, "Error reading address of A buffer", PMAC_TRAJ_BUFF_ADR_A);
      debug(DEBUG_ERROR, functionName, "    nvals", nvals);
      debug(DEBUG_ERROR, functionName, "    response", trajPtr);
      status = asynError;
    } else {
      // Save the value into the parameter
      setIntegerParam(PMAC_C_TrajBuffAdrA_, tScanPmacBufferAddressA_);
      debugf(DEBUG_VARIABLE, functionName, "Slow read trajectory address buffer A [%s] => %X", PMAC_TRAJ_BUFF_ADR_A, tScanPmacBufferAddressA_);
    }
  }

  // Read the address of the B half buffer
  trajPtr = sPtr->readValue(PMAC_TRAJ_BUFF_ADR_B);
  if (trajPtr == ""){
    debug(DEBUG_ERROR, functionName, "Problem reading address of B buffer", PMAC_TRAJ_BUFF_ADR_B);
    status = asynError;
  } else {
    nvals = sscanf(trajPtr.c_str(), "%d", &tScanPmacBufferAddressB_);
    if (nvals != 1) {
      debug(DEBUG_ERROR, functionName, "Error reading address of B buffer", PMAC_TRAJ_BUFF_ADR_B);
      debug(DEBUG_ERROR, functionName, "    nvals", nvals);
      debug(DEBUG_ERROR, functionName, "    response", trajPtr);
      status = asynError;
    } else {
      // Save the value into the parameter
      setIntegerParam(PMAC_C_TrajBuffAdrB_, tScanPmacBufferAddressB_);
      debugf(DEBUG_VARIABLE, functionName, "Slow read trajectory address buffer B [%s] => %X", PMAC_TRAJ_BUFF_ADR_B, tScanPmacBufferAddressB_);
    }
  }

  return status;
}

/**
 * Connect to the underlying low level Asyn port that is used for comms.
 * This uses the asynOctetSyncIO interface, and also sets the input and output terminators.
 * @param port The port to connect to
 * @param addr The address of the port to connect to
 * @param ppasynUser A pointer to the pasynUser structure used by the controller
 * @param inputEos The input EOS character
 * @param outputEos The output EOS character
 * @return asynStatus  
 */
/*asynStatus pmacController::lowLevelPortConnect(const char *port, int addr, asynUser **ppasynUser, char *inputEos, char *outputEos)
{
  asynStatus status = asynSuccess;
 
  static const char *functionName = "pmacController::lowLevelPortConnect";

  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s\n", functionName);

  status = pasynOctetSyncIO->connect( port, addr, ppasynUser, NULL);
  if (status) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
	      "pmacController::motorAxisAsynConnect: unable to connect to port %s\n", 
	      port);
    return status;
  }

  //Do I want to disconnect below? If the IP address comes up, will the driver recover
  //if the poller functions are running? Might have to use asynManager->isConnected to
  //test connection status of low level port (in the pollers). But then autosave 
  //restore doesn't work (and we would save wrong positions). So I need to 
  //have a seperate function(s) to deal with connecting after IOC init.

  status = pasynOctetSyncIO->setInputEos(*ppasynUser, inputEos, strlen(inputEos) );
  if (status) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
	      "pmacController: unable to set input EOS on %s: %s\n", 
	      port, (*ppasynUser)->errorMessage);
    pasynOctetSyncIO->disconnect(*ppasynUser);
    //Set my low level pasynUser pointer to NULL
    *ppasynUser = NULL;
    return status;
  }
  
  status = pasynOctetSyncIO->setOutputEos(*ppasynUser, outputEos, strlen(outputEos));
  if (status) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
	      "pmacController: unable to set output EOS on %s: %s\n", 
	      port, (*ppasynUser)->errorMessage);
    pasynOctetSyncIO->disconnect(*ppasynUser);
    //Set my low level pasynUser pointer to NULL
    *ppasynUser = NULL;
    return status;
  }
  
  return status;
}*/

/**
 * Utilty function to print the connected status of the low level asyn port.
 * @return asynStatus
 */
/*asynStatus pmacController::printConnectedStatus()
{
  asynStatus status = asynSuccess;
  int asynManagerConnected = 0;
  static const char *functionName = "pmacController::printConnectedStatus";
  
  if (lowLevelPortUser_) {
    status = pasynManager->isConnected(lowLevelPortUser_, &asynManagerConnected);
      if (status!=asynSuccess) {
      asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
		"pmacController: Error calling pasynManager::isConnected.\n");
      return status;
      } else {
	asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s isConnected: %d\n", functionName, asynManagerConnected);
    }
  }
  return status;
}*/

asynStatus pmacController::immediateWriteRead(const char *command, char *response)
{
  asynStatus status = asynSuccess;
  static const char *functionName = "immediateWriteRead";
  this->startTimer(DEBUG_TIMING, functionName);
  status = this->lowLevelWriteRead(command, response);
  this->stopTimer(DEBUG_TIMING, functionName, "PMAC write/read time");
  return status;
}

/**
 * Wrapper for asynOctetSyncIO write/read functions.
 * @param command - String command to send.
 * @response response - String response back.
 */
asynStatus pmacController::lowLevelWriteRead(const char *command, char *response)
{
  asynStatus status = asynSuccess;
  static const char *functionName = "lowLevelWriteRead";

  debug(DEBUG_FLOW, functionName);

  status = pBroker_->immediateWriteRead(command, response);
  if (status == asynSuccess){
    status = this->updateStatistics();
  }
  return status;
}


void pmacController::report(FILE *fp, int level)
{
  int axis = 0;
  pmacAxis *pAxis = NULL;

  fprintf(fp, "pmac motor driver %s, numAxes=%d, moving poll period=%f, idle poll period=%f\n", 
          this->portName, numAxes_, movingPollPeriod_, idlePollPeriod_);

  if (level > 0) {
    for (axis=0; axis<numAxes_; axis++) {
      pAxis = getAxis(axis);
      if (!pAxis) continue;
      fprintf(fp, "  axis %d\n"
                  "    scale = %d\n", 
              pAxis->axisNo_,
              pAxis->scale_);
    }
  }

  // Call the base class method
  asynMotorController::report(fp, level);
}

/**
 * Deal with controller specific epicsFloat64 params.
 * @param pasynUser
 * @param value
 * @param asynStatus
 */
asynStatus pmacController::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
  int function = pasynUser->reason;
  bool status = true;
  pmacAxis *pAxis = NULL;
  char command[PMAC_MAXBUF_] = {0};
  char response[PMAC_MAXBUF_] = {0};
  double encRatio = 1.0;
  epicsInt32 encposition = 0;
  const char *name[128];
	
  static const char *functionName = "writeFloat64";

  debug(DEBUG_FLOW, functionName);

  getParamName(function, name);
  debug(DEBUG_VARIABLE, functionName, "Parameter Updated", *name);
  pAxis = this->getAxis(pasynUser);
  if (!pAxis) {
    return asynError;
  }

  /* Set the parameter and readback in the parameter library. */
  status = (pAxis->setDoubleParam(function, value) == asynSuccess) && status;

  if (function == motorPosition_) {
    /*Set position on motor axis.*/            
    epicsInt32 position = (epicsInt32) floor(value*32/pAxis->scale_ + 0.5);
    
    sprintf(command, "#%dK M%d61=%d*I%d08 M%d62=%d*I%d08",
	     pAxis->axisNo_,
	     pAxis->axisNo_, position, pAxis->axisNo_, 
	     pAxis->axisNo_, position, pAxis->axisNo_ );

    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
	      "%s: Set axis %d on controller %s to position %f\n", 
	      functionName, pAxis->axisNo_, portName, value);
    
    if ( command[0] != 0 && status) {
      status = (lowLevelWriteRead(command, response) == asynSuccess) && status;
    }
                
    sprintf(command, "#%dJ/", pAxis->axisNo_);

    if (command[0] != 0 && status) {
      status = (lowLevelWriteRead(command, response) == asynSuccess) && status;
    }

    /*Now set position on encoder axis, if one is in use.*/
                
    if (pAxis->encoder_axis_) {
      getDoubleParam(motorEncoderRatio_,  &encRatio);
      encposition = (epicsInt32) floor((position*encRatio) + 0.5);
                  
      sprintf(command, "#%dK M%d61=%d*I%d08 M%d62=%d*I%d08",
	      pAxis->encoder_axis_,
	      pAxis->encoder_axis_, encposition, pAxis->encoder_axis_, 
	      pAxis->encoder_axis_, encposition, pAxis->encoder_axis_ );
                  
      asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
		"%s: Set encoder axis %d on controller %s to position %f\n", 
		functionName, pAxis->axisNo_, portName, value);
                  
      if (command[0] != 0 && status) {
	status = (lowLevelWriteRead(command, response) == asynSuccess) && status;
      }
                  
      sprintf(command, "#%dJ/", pAxis->encoder_axis_);
      //The lowLevelWriteRead will be done at the end of this function.
    }

    /*Now do an update, to get the new position from the controller.*/
    bool moving = true;
    pAxis->getAxisStatus(&moving);
  } 
  else if (function == motorLowLimit_) {
    sprintf(command, "I%d14=%f", pAxis->axisNo_, value/pAxis->scale_);
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
	      "%s: Setting low limit on controller %s, axis %d to %f\n",
	      functionName, portName, pAxis->axisNo_, value);
  }
  else if (function == motorHighLimit_) {
    sprintf(command, "I%d13=%f", pAxis->axisNo_, value/pAxis->scale_);
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
	      "%s: Setting high limit on controller %s, axis %d to %f\n",
	      functionName, portName, pAxis->axisNo_, value);
  } else if (pWriteParams_->hasKey(*name)){
    // This is an integer write of a parameter, so send the immediate write/read
    sprintf(command, "%s=%f", pWriteParams_->lookup(*name).c_str(), value);
    debug(DEBUG_VARIABLE, functionName, "Command sent to PMAC", command);
    status = (this->immediateWriteRead(command, response) == asynSuccess) && status;
  }

  if (command[0] != 0 && status) {
    status = (lowLevelWriteRead(command, response) == asynSuccess) && status;
  }

  //Call base class method. This will handle callCallbacks even if the function was handled here.
  status = (asynMotorController::writeFloat64(pasynUser, value) == asynSuccess) && status;

  if (!status) {
    setIntegerParam(pAxis->axisNo_, this->motorStatusCommsError_, PMAC_ERROR_);
    return asynError;
  } else {
    setIntegerParam(pAxis->axisNo_, this->motorStatusCommsError_, PMAC_OK_);
  }

  return asynSuccess;

}

/** Called when asyn clients call pasynOctet->write().
  * This function performs actions for some parameters, including AttributesFile.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Address of the string to write.
  * \param[in] nChars Number of characters to write.
  * \param[out] nActual Number of characters actually written. */
asynStatus pmacController::writeOctet(asynUser *pasynUser, const char *value, size_t nChars, size_t *nActual)
{
  int addr=0;
  int function = pasynUser->reason;
  asynStatus status = asynSuccess;
  char command[PMAC_MAXBUF_] = {0};
  char response[PMAC_MAXBUF_] = {0};
  const char *functionName = "writeOctet";

  status = getAddress(pasynUser, &addr); if (status != asynSuccess) return(status);
  // Set the parameter in the parameter library.
  status = (asynStatus)setStringParam(addr, function, (char *)value);
  if (status != asynSuccess) return(status);

  if (function == PMAC_C_WriteCmd_){
    // Write the arbitrary string to the PMAC, ignoring a reponse
    strcpy(command, value);
    status = this->immediateWriteRead(command, response);
  }

  // Do callbacks so higher layers see any changes
  callParamCallbacks(addr, addr);

  //Call base class method. This will handle callCallbacks even if the function was handled here.
  status = asynMotorController::writeOctet(pasynUser, value, nChars, nActual);

  if (status != asynSuccess){
    epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                  "%s:%s: status=%d, function=%d, value=%s",
                  driverName, functionName, status, function, value);
  } else {
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s:%s: function=%d, value=%s\n",
              driverName, functionName, function, value);
  }
  *nActual = nChars;
  return status;
}

/**
 * Deal with controller specific epicsInt32 params.
 * @param pasynUser
 * @param value
 * @param asynStatus
 */
asynStatus pmacController::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
  int function = pasynUser->reason;
  char command[PMAC_MAXBUF_] = {0};
  char response[PMAC_MAXBUF_] = {0};
  bool status = true;
  pmacAxis *pAxis = NULL;
  const char *name[128];
  static const char *functionName = "writeInt32";

  debug(DEBUG_TRACE, functionName);

  getParamName(function, name);
  debug(DEBUG_VARIABLE, functionName, "Parameter Updated", *name);
  pAxis = this->getAxis(pasynUser);
  if (!pAxis) {
    return asynError;
  } 

  status = (pAxis->setIntegerParam(function, value) == asynSuccess) && status;

  if (function == PMAC_C_FeedRatePoll_) {
    if (value) {
      this->feedRatePoll_ = true;
    } else {
      this->feedRatePoll_ = false;
    }
  } 
  else if (function == PMAC_C_FeedRate_) {
    sprintf(command, "%%%d", value);
    if (command[0] != 0) {
      //PMAC does not respond to this command.
      lowLevelWriteRead(command, response);
    }
  }
  else if (function == motorDeferMoves_) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s: Setting deferred move mode on PMAC %s to %d\n", functionName, portName, value);
    if (value == 0 && this->movesDeferred_ == DEFERRED_FAST_MOVES) {
      status = (this->processDeferredMoves() == asynSuccess) && status;
    }
    else if (value == 0 && this->movesDeferred_ == DEFERRED_COORDINATED_MOVES) {
    	status = (pGroupList->processDeferredCoordMoves() == asynSuccess) && status;
    }
    this->movesDeferred_ = value;
  }
  else if (function == PMAC_C_CoordSysGroup_) {
	  status = (pGroupList->switchToGroup(value) == asynSuccess) && status;
  } else if (pWriteParams_->hasKey(*name)){
    // This is an integer write of a parameter, so send the immediate write/read
    sprintf(command, "%s=%d", pWriteParams_->lookup(*name).c_str(), value);
    debug(DEBUG_VARIABLE, functionName, "Command sent to PMAC", command);
    status = (this->immediateWriteRead(command, response) == asynSuccess) && status;
  } else if (function == PMAC_C_KillAxis_){
    // Send the kill command to the PMAC immediately
    sprintf(command, "#%dk", pAxis->axisNo_);
    status = (this->immediateWriteRead(command, response) == asynSuccess) && status;
  }
  
  //Call base class method. This will handle callCallbacks even if the function was handled here.
  status = (asynMotorController::writeInt32(pasynUser, value) == asynSuccess) && status;

  if (!status) {
    setIntegerParam(pAxis->axisNo_, this->motorStatusCommsError_, PMAC_ERROR_);
    return asynError;
  } else {
    setIntegerParam(pAxis->axisNo_, this->motorStatusCommsError_, PMAC_OK_);
  }

  if (function == motorClosedLoop_) {
    bool closedLoop = (value == 0) ? 0 : 1;

    status = pAxis->setClosedLoop(closedLoop);
  }

  if (!status) {
    setIntegerParam(pAxis->axisNo_, this->motorStatusCommsError_, PMAC_ERROR_);
    return asynError;
  } else {
    setIntegerParam(pAxis->axisNo_, this->motorStatusCommsError_, PMAC_OK_);
  }

  return asynSuccess;

}


/** Returns a pointer to an pmacAxis object.
  * Returns NULL if the axis number encoded in pasynUser is invalid.
  * \param[in] pasynUser asynUser structure that encodes the axis index number. */
pmacAxis* pmacController::getAxis(asynUser *pasynUser)
{
  int axisNo = 0;
    
  getAddress(pasynUser, &axisNo);
  return getAxis(axisNo);
}



/** Returns a pointer to an pmacAxis object.
  * Returns NULL if the axis number is invalid.
  * \param[in] axisNo Axis index number. */
pmacAxis* pmacController::getAxis(int axisNo)
{
  if ((axisNo < 0) || (axisNo >= numAxes_)) return NULL;
  return pAxes_[axisNo];
}


/** 
 * Polls the controller, rather than individual axis.
 * @return asynStatus
 */
asynStatus pmacController::poll()
{
  char tBuff[32];
  static const char *functionName = "poll";
  debug(DEBUG_FLOW, functionName);

  epicsTimeGetCurrent(&nowTime_);
  // Always call for a fast update
  debug(DEBUG_TRACE, functionName, "Fast update has been called", tBuff);
  pBroker_->updateVariables(pmacMessageBroker::PMAC_FAST_READ);
  this->updateStatistics();
  setDoubleParam(PMAC_C_FastUpdateTime_, pBroker_->readUpdateTime());

  if (epicsTimeDiffInSeconds(&nowTime_, &lastMediumTime_) >= PMAC_MEDIUM_LOOP_TIME/1000.0){
    epicsTimeAddSeconds(&lastMediumTime_, PMAC_MEDIUM_LOOP_TIME/1000.0);
    epicsTimeToStrftime(tBuff, 32, "%Y/%m/%d %H:%M:%S.%03f", &nowTime_);
    debug(DEBUG_TRACE, functionName, "Medium update has been called", tBuff);
    pBroker_->updateVariables(pmacMessageBroker::PMAC_MEDIUM_READ);
  }
  if (epicsTimeDiffInSeconds(&nowTime_, &lastSlowTime_) >= PMAC_SLOW_LOOP_TIME/1000.0){
    epicsTimeAddSeconds(&lastSlowTime_, PMAC_SLOW_LOOP_TIME/1000.0);
    epicsTimeToStrftime(tBuff, 32, "%Y/%m/%d %H:%M:%S.%03f", &nowTime_);
    debug(DEBUG_TRACE, functionName, "Slow update has been called", tBuff);
    pBroker_->updateVariables(pmacMessageBroker::PMAC_SLOW_READ);
  }

  return asynSuccess;

  /*
  if (!lowLevelPortUser_) {
    return asynError;
  }
*/

  /* Get the time and decide if we want to print errors.*/
/*  epicsTimeGetCurrent(&nowTime_);
  nowTimeSecs_ = nowTime_.secPastEpoch;
  if ((nowTimeSecs_ - lastTimeSecs_) < PMAC_ERROR_PRINT_TIME_) {
    printErrors = 0;
  } else {
    printErrors = 1;
    lastTimeSecs_ = nowTimeSecs_;
  }

  if (printNextError_) {
    printErrors = 1;
  }
  
  //Set any controller specific parameters. 
  //Some of these may be used by the axis poll to set axis problem bits.
  status = (getGlobalStatus(&globalStatus, &feedrate, feedRatePoll_) == asynSuccess) && status;
  hardwareProblem = ((globalStatus & PMAC_HARDWARE_PROB) != 0);
  status = (setIntegerParam(this->PMAC_C_GlobalStatus_, hardwareProblem) == asynSuccess) && status;
  if(hardwareProblem){
	  asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s: *** Hardware Problem *** globalstatus = %x\n",
			  functionName, globalStatus);
  }

  if (status && feedRatePoll_) {
    status = (setIntegerParam(this->PMAC_C_FeedRate_, feedrate) == asynSuccess) && status;
    status = (getIntegerParam(this->PMAC_C_FeedRateLimit_, &feedrate_limit) == asynSuccess) && status;
    if (feedrate < static_cast<int>(feedrate_limit-PMAC_FEEDRATE_DEADBAND_)) {
      status = (setIntegerParam(this->PMAC_C_FeedRateProblem_, PMAC_ERROR_) == asynSuccess) && status;
      if (printErrors) {
	asynPrint(lowLevelPortUser_, ASYN_TRACE_ERROR, 
		  "%s: *** ERROR ***: global feed rate below limit. feedrate: %d, feedrate limit %d\n", functionName, feedrate, feedrate_limit);
	printNextError_ = false;
      }
    } else {
      status = (setIntegerParam(this->PMAC_C_FeedRateProblem_, PMAC_OK_) == asynSuccess) && status;
      printNextError_ = true;
    }
  }
  
  callParamCallbacks();

  if (!status) {
    asynPrint(lowLevelPortUser_, ASYN_TRACE_ERROR, "%s: Error reading or setting params.\n", functionName);
    setIntegerParam(PMAC_C_CommsError_, PMAC_ERROR_);
    return asynError;
  } else {
    setIntegerParam(PMAC_C_CommsError_, PMAC_OK_);
    return asynSuccess;
  }*/
}

asynStatus pmacController::initializeProfile(size_t maxPoints)
{
  static const char *functionName = "initializeProfile";

  debug(DEBUG_ERROR, functionName);
  debug(DEBUG_ERROR, functionName, "maxPoints", (int)maxPoints);

  // Allocate the pointers
  tScanPositions_ = (double **)malloc(sizeof(double *) * PMAC_MAX_CS_AXES);
  // Now allocate each position array
  for (int axis = 0; axis < PMAC_MAX_CS_AXES; axis++){
    tScanPositions_[axis] = (double *)malloc(sizeof(double) * maxPoints);
  }
  // Finally call super class
  return asynMotorController::initializeProfile(maxPoints);
}

asynStatus pmacController::buildProfile(int csNo)
{
  asynStatus status = asynSuccess;
  int numPoints = 0;
  int axisMask = 0;
  static const char *functionName = "buildProfile";

  debug(DEBUG_ERROR, functionName);

  // Set the build state to busy
  setIntegerParam(profileBuildState_, PROFILE_BUILD_BUSY);
  // Set the build status to undefined
  setIntegerParam(profileBuildStatus_, PROFILE_STATUS_UNDEFINED);
  // Set the message accordingly
  setStringParam(profileBuildMessage_, "Building profile");
  callParamCallbacks();

  // First check to see if we need to initialise memory
  if (!profileInitialized_){
    // Initialise the trajectory scan interface pointers
    status = this->initializeProfile(PMAC_MAX_TRAJECTORY_POINTS);
    if (status == asynSuccess){
      profileInitialized_ = true;
    } else {
      debug(DEBUG_ERROR, functionName, "Failed to allocate memory on controller");
      // Set the build state to done
      setIntegerParam(profileBuildState_, PROFILE_BUILD_DONE);
      // Set the build status to failure
      setIntegerParam(profileBuildStatus_, PROFILE_STATUS_FAILURE);
      // Set the message accordingly
      setStringParam(profileBuildMessage_, "Failed to allocate memory on controller");
    }
  }

  // Check the CS number is valid
  if (csNo > PMAC_MAX_CS){
    debug(DEBUG_ERROR, functionName, "Invalid CS number", csNo);
    // Set the build state to done
    setIntegerParam(profileBuildState_, PROFILE_BUILD_DONE);
    // Set the build status to failure
    setIntegerParam(profileBuildStatus_, PROFILE_STATUS_FAILURE);
    // Set the message accordingly
    setStringParam(profileBuildMessage_, "Invalid CS number requested build");
    status = asynError;
  }
  if (status == asynSuccess){
    if (pCSControllers_[csNo] == NULL){
      debug(DEBUG_ERROR, functionName, "Invalid CS number", csNo);
      // Set the build state to done
      setIntegerParam(profileBuildState_, PROFILE_BUILD_DONE);
      // Set the build status to failure
      setIntegerParam(profileBuildStatus_, PROFILE_STATUS_FAILURE);
      // Set the message accordingly
      setStringParam(profileBuildMessage_, "Invalid CS number requested build");
      status = asynError;
    }
  }

  // Verify that we are not currently executing a trajectory scan
  if (status == asynSuccess){
    if (tScanExecuting_ > 0){
      // Set the build state to done
      setIntegerParam(profileBuildState_, PROFILE_BUILD_DONE);
      // Set the build status to failure
      setIntegerParam(profileBuildStatus_, PROFILE_STATUS_FAILURE);
      // Set the message accordingly
      setStringParam(profileBuildMessage_, "Scan is already executing");
      status = asynError;
    }
  }

  // Compile the required arrays for time, position, user
  if (status == asynSuccess){
    // Set the current scan CS
    tScanCSNo_ = csNo;
    debug(DEBUG_ERROR, functionName, "Current scan CS", tScanCSNo_);
    // Copy the time array
    status = pCSControllers_[tScanCSNo_]->tScanBuildTimeArray(profileTimes_, &numPoints, PMAC_MAX_TRAJECTORY_POINTS);
    tScanNumPoints_ = numPoints;
    if (status != asynSuccess){
      // Set the build state to done
      setIntegerParam(profileBuildState_, PROFILE_BUILD_DONE);
      // Set the build status to failure
      setIntegerParam(profileBuildStatus_, PROFILE_STATUS_FAILURE);
      // Set the message accordingly
      setStringParam(profileBuildMessage_, "Failed to build profile times");
    } else {
      // Ask the CS controller for the bitmap of axes that are to be included in the scan
      // 1 to 9 axes (0 is error) 111111111 => 1 .. 511
      status = pCSControllers_[tScanCSNo_]->tScanIncludedAxes(&axisMask);
      tScanAxisMask_ = axisMask;
      //Check if each axis from the coordinate system is involved in this trajectory scan
      for (int index = 0; index < PMAC_MAX_CS_AXES; index++){
        if ((1<<index & axisMask) > 0){
          if (status == asynSuccess){
            // If the axis is going to be included then copy the position array into local
            // storage ready for the trajectory execution
            int axis = index+1; // axis is 1 index based
            status = pCSControllers_[tScanCSNo_]->tScanBuildProfileArray(tScanPositions_[index], axis, numPoints);
            if (status != asynSuccess){
              // Set the build state to done
              setIntegerParam(profileBuildState_, PROFILE_BUILD_DONE);
              // Set the build status to failure
              setIntegerParam(profileBuildStatus_, PROFILE_STATUS_FAILURE);
              // Set the message accordingly
              setStringParam(profileBuildMessage_, "Failed to build profile positions");
            }
          }
        }
      }
    }
  }

  // Finally if the profile build has completed then set the status accordingly
  if (status == asynSuccess){
    // Set the number of points in the scan
    setIntegerParam(profileNumPoints_, tScanNumPoints_);
    // Set the build state to done
    setIntegerParam(profileBuildState_, PROFILE_BUILD_DONE);
    // Set the build status to failure
    setIntegerParam(profileBuildStatus_, PROFILE_STATUS_SUCCESS);
    // Set the message accordingly
    setStringParam(profileBuildMessage_, "Profile built");
  } else {
    // Zero the number of points in the scan
    setIntegerParam(profileNumPoints_, 0);
  }
  callParamCallbacks();

  return status;
}

asynStatus pmacController::executeProfile(int csNo)
{
  asynStatus status = asynSuccess;
  int buildState = 0;
  int buildStatus = 0;
  static const char *functionName = "executeProfile";

  debug(DEBUG_ERROR, functionName);

  // Set the execute state to move start
  setIntegerParam(profileExecuteState_, PROFILE_EXECUTE_MOVE_START);
  // Set the execute status to undefined
  setIntegerParam(profileExecuteStatus_, PROFILE_STATUS_UNDEFINED);
  // Set the message accordingly
  setStringParam(profileExecuteMessage_, "Starting trajectory execution");
  callParamCallbacks();

  // Check that the profile has been built successfully
  getIntegerParam(profileBuildState_, &buildState);
  getIntegerParam(profileBuildStatus_, &buildStatus);
  if ((buildStatus != PROFILE_STATUS_SUCCESS) || (buildState != PROFILE_BUILD_DONE)){
    // Set the execute state to done
    setIntegerParam(profileExecuteState_, PROFILE_EXECUTE_DONE);
    // Set the execute status to failure
    setIntegerParam(profileExecuteStatus_, PROFILE_STATUS_FAILURE);
    // Set the message accordingly
    setStringParam(profileExecuteMessage_, "Trajectory profile was not built successfully");
    debug(DEBUG_ERROR, functionName, "Trajectory was not built successfully");
    status = asynError;
    callParamCallbacks();
  }

  // Check that the same CS that built the profile has asked to execute
  // the trajectory scan
  if (status == asynSuccess){
    if (csNo != tScanCSNo_){
      // Set the execute state to done
      setIntegerParam(profileExecuteState_, PROFILE_EXECUTE_DONE);
      // Set the execute status to failure
      setIntegerParam(profileExecuteStatus_, PROFILE_STATUS_FAILURE);
      // Set the message accordingly
      setStringParam(profileExecuteMessage_, "Build and execute called by different CS");
      debugf(DEBUG_ERROR, functionName, "Profile built by CS %d but execute was called by CS %d", tScanCSNo_, csNo);
      status = asynError;
      callParamCallbacks();
    }
  }

  // Verify that we are not currently executing a trajectory scan
  if (status == asynSuccess){
    if (tScanExecuting_ > 0){
      // Set the execute state to done
      setIntegerParam(profileExecuteState_, PROFILE_EXECUTE_DONE);
      // Set the execute status to failure
      setIntegerParam(profileExecuteStatus_, PROFILE_STATUS_FAILURE);
      // Set the message accordingly
      setStringParam(profileExecuteMessage_, "Scan is already executing");
      status = asynError;
      callParamCallbacks();
    }
  }

  if (status == asynSuccess){
    // Checks have passed.
    // Send the signal to the trajectory thread
    epicsEventSignal(this->startEventId_);
  }
  return status;
}

asynStatus pmacController::abortProfile()
{
  asynStatus status = asynSuccess;
  char cmd[1024];
  char response[1024];
  const char *functionName = "trajectoryTask";

  debug(DEBUG_TRACE, functionName);

  // Send an immediate abort signal to the PMAC
  sprintf(cmd, "%s=1", PMAC_TRAJ_ABORT);
  status = this->immediateWriteRead(cmd, response);

  // Set the scan executing variable to 0
  tScanExecuting_ = 0;

  // Send the signal to the trajectory thread
  epicsEventSignal(this->stopEventId_);

  // Set the execute state to done
  setIntegerParam(profileExecuteState_, PROFILE_EXECUTE_DONE);
  // Set the execute status to success
  setIntegerParam(profileExecuteStatus_, PROFILE_STATUS_ABORT);
  // Set the message accordingly
  setStringParam(profileExecuteMessage_, "Trajectory scan aborted");
  callParamCallbacks();

  return status;
}

void pmacController::trajectoryTask()
{
  int status = asynSuccess;
  epicsTimeStamp startTime, endTime;
  double elapsedTime;
  int epicsBufferNumber = 0;
  int axisMask = 0;
  int progRunning = 0;
  char response[1024];
  char cmd[1024];
  const char *functionName = "trajectoryTask";

  this->lock();
  // Loop forever
  while (1) {
    // If we are not scanning then wait for a semaphore that is given when a scan is started
    if (!tScanExecuting_){
      // Release the lock while we wait for an event that says scan has started, then lock again
      debug(DEBUG_TRACE, functionName, "Waiting for scan to start");
      this->unlock();
      status = epicsEventWait(this->startEventId_);
      this->lock();
      tScanExecuting_ = 1;

      debug(DEBUG_TRACE, functionName, "Trajectory scan started");

      // Set the trajectory CS number
      setIntegerParam(PMAC_C_TrajCSNumber_, tScanCSNo_);

      // Reset the scan point counter
      tScanPointCtr_ = 0;

      // Reset the epicsBuffer number
      epicsBufferNumber = 0;

      // Send the initial half buffer of position updates
      status = this->sendTrajectoryDemands(epicsBufferNumber);

      // Record the scan start time
      epicsTimeGetCurrent(&startTime);

      // Calculate the axis mask ready to send to the PMAC
      // X => 256
      // Y => 128
      // Z => 64
      // U => 32
      // V => 16
      // W => 8
      // A => 4
      // B => 2
      // C => 1
      axisMask = 0;
      for (int index = 0; index < PMAC_MAX_CS_AXES; index++){
        if ((1<<index & tScanAxisMask_) > 0){
          // Bits swapped from EPICS axis mask
          axisMask += (1 << (8 - index));
        }
      }
      sprintf(cmd, "%s=%d", PMAC_TRAJ_AXES, axisMask);
      debug(DEBUG_ERROR, functionName, "Axis mask to send to PMAC (P4003)", axisMask);
      this->immediateWriteRead(cmd, response);

      // Abort any current move to make sure axes are enabled
      sprintf(cmd, "&%dA", tScanCSNo_);
      debug(DEBUG_ERROR, functionName, "Sending command to abort previous move", cmd);
      this->immediateWriteRead(cmd, response);
      // We are ready to execute the start demand
      sprintf(cmd, "&%dB%dR", tScanCSNo_, 1); // TODO: motion program should not be hardcoded to 1
      debug(DEBUG_ERROR, functionName, "Sending command to start the trajectory move", cmd);
      this->immediateWriteRead(cmd, response);

      // Reset our status
      setIntegerParam(PMAC_C_TrajEStatus_, 0);
      // Set the execute state to move start
      setIntegerParam(profileExecuteState_, PROFILE_EXECUTE_EXECUTING);
      // Set the execute status to undefined
      setIntegerParam(profileExecuteStatus_, PROFILE_STATUS_SUCCESS);
      // Set the message accordingly
      setStringParam(profileExecuteMessage_, "Executing trajectory scan");
      callParamCallbacks();
    }

    // Check if the reported PMAC buffer number is the same as the EPICS buffer number
    if (tScanPmacBufferNumber_ == epicsBufferNumber){
      debug(DEBUG_ERROR, functionName, "Reading from buffer", tScanPmacBufferNumber_);
      debug(DEBUG_ERROR, functionName, "Send next demand set to PMAC");
      if (epicsBufferNumber == 0){
        epicsBufferNumber = 1;
      } else {
        epicsBufferNumber = 0;
      }
      // EPICS buffer number has just been updated, so fill the next
      // half buffer with positions
      status = this->sendTrajectoryDemands(epicsBufferNumber);
    }

    // Record the current scan time
    epicsTimeGetCurrent(&endTime);
    // Work out the elapsed time of the scan
    elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);
    setDoubleParam(PMAC_C_TrajRunTime_, elapsedTime);

    // Check if the scan has stopped/finished (status from PMAC)
    // Only start checking this value after we have been running for
    // long enough to be reading the current scan value
    if (elapsedTime > 2.0 && tScanPmacStatus_ != PMAC_TRAJ_STATUS_RUNNING){
      debug(DEBUG_ERROR, functionName, "tScanPmacStatus", tScanPmacStatus_);
      // Something has happened on the PMAC side
      tScanExecuting_ = 0;
      if (tScanPmacStatus_ == PMAC_TRAJ_STATUS_FINISHED){
        // Set the execute state to done
        setIntegerParam(profileExecuteState_, PROFILE_EXECUTE_DONE);
        // Set the execute status to success
        setIntegerParam(profileExecuteStatus_, PROFILE_STATUS_SUCCESS);
        // Set the message accordingly
        setStringParam(profileExecuteMessage_, "Trajectory scan complete");
      } else {
        // Set the execute state to done
        setIntegerParam(profileExecuteState_, PROFILE_EXECUTE_DONE);
        // Set the execute status to failure
        setIntegerParam(profileExecuteStatus_, PROFILE_STATUS_FAILURE);
        // Set the message accordingly
        setStringParam(profileExecuteMessage_, "Trajectory scan failed, motion program timeout start");
      }
      callParamCallbacks();
    }

    // Check here if the scan status reported by the PMAC is running but
    // the motion program is not running, this points to some other error
    // (possibly motor in limit)
    if (elapsedTime > 2.0 && tScanPmacStatus_ == PMAC_TRAJ_STATUS_RUNNING){
      if (pCSControllers_[tScanCSNo_]->tScanCheckProgramRunning(&progRunning) == asynSuccess){
        if (progRunning == 0){
          // Program not running but it should be
          tScanExecuting_ = 0;
          // Set the execute state to done
          setIntegerParam(profileExecuteState_, PROFILE_EXECUTE_DONE);
          // Set the execute status to failure
          setIntegerParam(profileExecuteStatus_, PROFILE_STATUS_FAILURE);
          // Set the message accordingly
          setStringParam(profileExecuteMessage_, "Trajectory scan failed, motion program not running");
        }
      }
    }

    // Here we need to check for CS errors that would abort the scan
    if (elapsedTime > 2.0 && pCSControllers_[tScanCSNo_]->tScanCheckForErrors() != asynSuccess){
      // There has been a CS error reported.  Abort the scan
      tScanExecuting_ = 0;
      // Set the status to 1 here, error detected
      setIntegerParam(PMAC_C_TrajEStatus_, 1);
      // Set the execute state to done
      setIntegerParam(profileExecuteState_, PROFILE_EXECUTE_DONE);
      // Set the execute status to failure
      setIntegerParam(profileExecuteStatus_, PROFILE_STATUS_FAILURE);
      // Set the message accordingly
      setStringParam(profileExecuteMessage_, "Trajectory scan failed, coordinate system error");
      callParamCallbacks();
    }

    // If we are scanning then sleep for a short period before checking the status
    if (tScanExecuting_){
      debug(DEBUG_FLOW, functionName, "Trajectory scan waiting");
      this->unlock();
      status = epicsEventWaitWithTimeout(this->stopEventId_, 0.1);
      this->lock();
    }

    // TODO: Check the total points
    // TODO: Check the current point
    // TODO: Check the PMAC buffer and point
    // TODO: Work out how many points we can write in a single message
    // TODO: Fill a buffer
    // TODO: Wait for next available buffer
  }
}

asynStatus pmacController::sendTrajectoryDemands(int buffer)
{
  asynStatus status = asynSuccess;
  int nAxes = 0;
  int nBuffers = 0;
  int epicsBufferPtr = 0;
  int writeAddress = 0;
  char response[1024];
  const char *functionName = "sendTrajectoryDemands";

  startTimer(DEBUG_ERROR, functionName);

  // Calculate how many axes are included in this trajectory scan
  nAxes = 0;
  for (int index = 0; index < PMAC_MAX_CS_AXES; index++){
    if ((1<<index & tScanAxisMask_) > 0){
      nAxes++;
    }
  }

  // Calculate how many points can be written into a single message
  // nAxes + 1 is due to the extra time and user buffer
  nBuffers = PMAC_POINTS_PER_WRITE / (nAxes+1);


  // Check the number of points we have, if greater than the buffer size
  // then fill the buffer, else fill up to the number of points
  while (epicsBufferPtr < tScanPmacBufferSize_ && tScanPointCtr_ < tScanNumPoints_){
    // Set the address of the write according to the half buffer
    if (buffer == PMAC_TRAJ_BUFFER_A){
      writeAddress = tScanPmacBufferAddressA_;
    } else if (buffer == PMAC_TRAJ_BUFFER_B){
      writeAddress = tScanPmacBufferAddressB_;
    } else {
      // TODO: ERROR!!
    }
    // Offset the write address by the epics buffer pointer
    writeAddress += epicsBufferPtr;

    // Count how many buffers to fill
    char cmd[11][1024];
    // cmd[9] is reserved for the time values
    sprintf(cmd[9], "WL:$%X", writeAddress);

    // cmd[0..8] are reserved for axis positions
    for (int index = 0; index < PMAC_MAX_CS_AXES; index++){
      if ((1<<index & tScanAxisMask_) > 0){
        sprintf(cmd[index], "WL:$%X", writeAddress+((index+1)*(tScanPmacBufferSize_)));
      }
    }

    int bufferCount = 0;
    while ((bufferCount < nBuffers) && (epicsBufferPtr < tScanPmacBufferSize_) && (tScanPointCtr_ < tScanNumPoints_)){
      // TODO: First 24 bits (X) should be written for user buffer
      // TODO: Currently hardcoded to 0
      sprintf(cmd[9], "%s,$%X%06X", cmd[9], 0, (int)profileTimes_[tScanPointCtr_]);
      for (int index = 0; index < PMAC_MAX_CS_AXES; index++){
        if ((1<<index & tScanAxisMask_) > 0){
          int64_t ival = 0;
          doubleToPMACFloat(tScanPositions_[index][tScanPointCtr_], &ival);
//          sprintf(cmd[index], "%s,$%X", cmd[index], (int)tScanPositions_[index][tScanPointCtr_]);
          sprintf(cmd[index], "%s,$%lX", cmd[index], ival);
        }
      }
      // Increment the scan point counter
      tScanPointCtr_++;
      // Increment the buffer count
      bufferCount++;
      // Increment the epicsBufferPtr
      epicsBufferPtr++;
    }

    // Construct the final cmd string
    char cstr[1024];
    sprintf(cstr, "%s", cmd[9]);
    for (int index = 0; index < PMAC_MAX_CS_AXES; index++){
      if ((1<<index & tScanAxisMask_) > 0){
        sprintf(cstr, "%s %s", cstr, cmd[index]);
      }
    }

    // Append the current buffer pointer to the command string ready to send to the PMAC
    if (buffer == PMAC_TRAJ_BUFFER_A){
      sprintf(cstr, "%s %s=%d", cstr, PMAC_TRAJ_BUFF_FILL_A, epicsBufferPtr);
    } else if (buffer == PMAC_TRAJ_BUFFER_B){
      sprintf(cstr, "%s %s=%d", cstr, PMAC_TRAJ_BUFF_FILL_B, epicsBufferPtr);
    } else {
      // TODO: ERROR!!
    }
    this->unlock();
    status = this->immediateWriteRead(cstr, response);
    this->lock();
    // Set the parameter according to the filled points
    if (buffer == PMAC_TRAJ_BUFFER_A){
      setIntegerParam(PMAC_C_TrajBuffFillA_, epicsBufferPtr);
    } else if (buffer == PMAC_TRAJ_BUFFER_B){
      setIntegerParam(PMAC_C_TrajBuffFillB_, epicsBufferPtr);
    } else {
      // TODO: ERROR!!
    }

    debug(DEBUG_FLOW, functionName, "Command", cstr);
  }

  stopTimer(DEBUG_ERROR, functionName, "Time taken to send trajectory demand");

  return status;
}

asynStatus pmacController::doubleToPMACFloat(double value, int64_t *representation)
{
  asynStatus status = asynSuccess;
  double absVal = value;
  int negative = 0;
  int exponent = 0;
  double expVal = 0.0;
  int64_t intVal = 0;
  int64_t tVal = 0;
  double mantissaVal = 0.0;
  double maxMantissa = 34359738368.0;  // 0x800000000
  const char *functionName = "doubleToPMACFloat";

  debug(DEBUG_TRACE, functionName);
  debugf(DEBUG_VARIABLE, functionName, "Value : %20.10lf\n", value);

  // Check for special case 0.0
  if (absVal == 0.0){
    // Set value accordingly
    tVal = 0x0;
  } else {
    // Check for a negative number, and get the absolute
    if (absVal < 0.0){
      absVal = absVal * -1.0;
      negative = 1;
    }
    expVal = absVal;
    mantissaVal = absVal;

    // Work out the exponent required to normalise
    // Normalised should be between 1 and 2
    while (expVal >= 2.0){
      expVal = expVal / 2.0;
      exponent++;
    }
    while (expVal < 1.0){
      expVal = expVal * 2.0;
      exponent--;
    }
    // Offset exponent to provide +-2048 range
    exponent += 0x800;

    // Get the mantissa into correct format, this might not be
    // the most efficient way to do this
    while (mantissaVal < maxMantissa){
      mantissaVal *= 2.0;
    }
    mantissaVal = mantissaVal / 2.0;
    // Get the integer representation for the altered mantissa
    intVal = (int64_t)mantissaVal;

    // If negative value then subtract altered mantissa from max
    if (negative == 1){
      intVal = 0xFFFFFFFFFLL - intVal;
    }

    // Shift the altered mantissa by 12 bits and then set those
    // 12 bits to the offset exponent
    tVal = intVal << 12;
    tVal += exponent;
  }

  *representation = tVal;

  debugf(DEBUG_VARIABLE, functionName, "Prepared value: %12lX\n", tVal);

  return status;
}

asynStatus pmacController::newGetGlobalStatus(pmacCommandStore *sPtr)
{
  asynStatus status = asynSuccess;
  epicsUInt32 globalStatus = 0;
  int feedrate = 0;
  int feedrate_limit = 0;
  bool printErrors = 0;
  bool hardwareProblem;
  int nvals;
  std::string trajBufPtr = "";
  static const char *functionName = "getGlobalStatus";

  // Get the time and decide if we want to print errors.
  epicsTimeGetCurrent(&nowTime_);
  nowTimeSecs_ = nowTime_.secPastEpoch;
  if ((nowTimeSecs_ - lastTimeSecs_) < PMAC_ERROR_PRINT_TIME_) {
    printErrors = 0;
  } else {
    printErrors = 1;
    lastTimeSecs_ = nowTimeSecs_;
  }

  if (printNextError_) {
    printErrors = 1;
  }


  // Read the current trajectory status from the PMAC
  trajBufPtr = sPtr->readValue(PMAC_TRAJ_STATUS);
  if (trajBufPtr == ""){
    debug(DEBUG_ERROR, functionName, "Problem reading trajectory status", PMAC_TRAJ_STATUS);
    status = asynError;
  } else {
    nvals = sscanf(trajBufPtr.c_str(), "%d", &tScanPmacStatus_);
    if (nvals != 1) {
      debug(DEBUG_ERROR, functionName, "Error reading trajectory status", PMAC_TRAJ_STATUS);
      debug(DEBUG_ERROR, functionName, "    nvals", nvals);
      debug(DEBUG_ERROR, functionName, "    response", trajBufPtr);
      status = asynError;
    } else {
      // Save the value into the parameter
      setIntegerParam(PMAC_C_TrajStatus_, tScanPmacStatus_);
      debugf(DEBUG_VARIABLE, functionName, "Fast read trajectory status [%s] => %d", PMAC_TRAJ_STATUS, tScanPmacStatus_);
    }
  }

  // Read the current trajectory buffer index read from the PMAC (within current buffer)
  trajBufPtr = sPtr->readValue(PMAC_TRAJ_CURRENT_INDEX);
  if (trajBufPtr == ""){
    debug(DEBUG_ERROR, functionName, "Problem reading trajectory current index", PMAC_TRAJ_CURRENT_INDEX);
    status = asynError;
  } else {
    nvals = sscanf(trajBufPtr.c_str(), "%d", &tScanPmacBufferPtr_);
    if (nvals != 1) {
      debug(DEBUG_ERROR, functionName, "Error reading trajectory current index", PMAC_TRAJ_CURRENT_INDEX);
      debug(DEBUG_ERROR, functionName, "    nvals", nvals);
      debug(DEBUG_ERROR, functionName, "    response", trajBufPtr);
      status = asynError;
    } else {
      // Save the value into the parameter
      setIntegerParam(PMAC_C_TrajCurrentIndex_, tScanPmacBufferPtr_);
      debugf(DEBUG_VARIABLE, functionName, "Fast read trajectory current index [%s] => %d", PMAC_TRAJ_CURRENT_INDEX, tScanPmacBufferPtr_);
    }
  }

  // Read the current trajectory total number of points from the PMAC
  trajBufPtr = sPtr->readValue(PMAC_TRAJ_TOTAL_POINTS);
  if (trajBufPtr == ""){
    debug(DEBUG_ERROR, functionName, "Problem reading trajectory total points", PMAC_TRAJ_TOTAL_POINTS);
    status = asynError;
  } else {
    nvals = sscanf(trajBufPtr.c_str(), "%d", &tScanPmacTotalPts_);
    if (nvals != 1) {
      debug(DEBUG_ERROR, functionName, "Error reading trajectory current index", PMAC_TRAJ_TOTAL_POINTS);
      debug(DEBUG_ERROR, functionName, "    nvals", nvals);
      debug(DEBUG_ERROR, functionName, "    response", trajBufPtr);
      status = asynError;
    } else {
      // Save the value into the parameter
      setIntegerParam(PMAC_C_TrajTotalPoints_, tScanPmacTotalPts_);
      debugf(DEBUG_VARIABLE, functionName, "Fast read trajectory total points [%s] => %d", PMAC_TRAJ_TOTAL_POINTS, tScanPmacTotalPts_);
      // Now work out the percent complete
      double pctComplete = 0.0;
      if (tScanNumPoints_ > 0){
        pctComplete = (double)tScanPmacTotalPts_ * 100.0 / (double)tScanNumPoints_;
      }
      setDoubleParam(PMAC_C_TrajPercent_, pctComplete);
    }
  }

  // Read the current trajectory buffer (A=0,B=1) being read by the PMAC
  trajBufPtr = sPtr->readValue(PMAC_TRAJ_CURRENT_BUFFER);
  if (trajBufPtr == ""){
    debug(DEBUG_ERROR, functionName, "Problem reading trajectory current buffer", PMAC_TRAJ_CURRENT_BUFFER);
    status = asynError;
  } else {
    nvals = sscanf(trajBufPtr.c_str(), "%d", &tScanPmacBufferNumber_);
    if (nvals != 1) {
      debug(DEBUG_ERROR, functionName, "Error reading trajectory current buffer", PMAC_TRAJ_CURRENT_BUFFER);
      debug(DEBUG_ERROR, functionName, "    nvals", nvals);
      debug(DEBUG_ERROR, functionName, "    response", trajBufPtr);
      status = asynError;
    } else {
      // Save the value into the parameter
      setIntegerParam(PMAC_C_TrajCurrentBuffer_, tScanPmacBufferNumber_);
      debugf(DEBUG_VARIABLE, functionName, "Fast read trajectory current buffer [%s] => %d", PMAC_TRAJ_CURRENT_BUFFER, tScanPmacBufferNumber_);
    }
  }

  // Lookup the value of global status
  std::string globStatus = sPtr->readValue("???");
  debug(DEBUG_VARIABLE, functionName, "Global status [???]", globStatus);

  // Check the global status value is valid
  if (globStatus == ""){
    debug(DEBUG_ERROR, functionName, "Problem reading global status command ???");
    status = asynError;
  } else {
    nvals = sscanf(globStatus.c_str(), "%6x", &globalStatus);
    if (nvals != 1) {
      debug(DEBUG_ERROR, functionName, "Error reading global status [???]");
      debug(DEBUG_ERROR, functionName, "    nvals", nvals);
      debug(DEBUG_ERROR, functionName, "    response", globStatus);
      status = asynError;
    }
  }

  // Lookup the value of the feedrate
  std::string feedRate = sPtr->readValue("%");
  debug(DEBUG_VARIABLE, functionName, "Feedrate [%]", feedRate);

  // Check the feedrate value is valid
  if (feedRate == ""){
    debug(DEBUG_ERROR, functionName, "Problem reading feed rate command %");
    status = asynError;
  } else {
    nvals = sscanf(feedRate.c_str(), "%d", &feedrate);
    if (nvals != 1) {
      debug(DEBUG_ERROR, functionName, "Error reading feedrate [%]");
      debug(DEBUG_ERROR, functionName, "    nvals", nvals);
      debug(DEBUG_ERROR, functionName, "    response", feedRate);
      status = asynError;
    }
  }

  //Set any controller specific parameters.
  //Some of these may be used by the axis poll to set axis problem bits.
  if (status == asynSuccess){
    hardwareProblem = ((globalStatus & PMAC_HARDWARE_PROB) != 0);
    status = setIntegerParam(this->PMAC_C_GlobalStatus_, hardwareProblem);
    if(hardwareProblem){
      debug(DEBUG_ERROR, functionName, "*** Hardware Problem *** global status [???]", (int)globalStatus);
    }
  }
  if (status == asynSuccess){
    status = setIntegerParam(this->PMAC_C_FeedRate_, feedrate);
  }
  if (status == asynSuccess){
    status = getIntegerParam(this->PMAC_C_FeedRateLimit_, &feedrate_limit);
  }

  if (status == asynSuccess){
    if (feedrate < static_cast<int>(feedrate_limit-PMAC_FEEDRATE_DEADBAND_)) {
      status = setIntegerParam(this->PMAC_C_FeedRateProblem_, PMAC_ERROR_);
      if (printErrors){
        debug(DEBUG_ERROR, functionName, "*** ERROR ***: global feed rate below limit.");
        debug(DEBUG_ERROR, functionName, "               feedrate", feedrate);
        debug(DEBUG_ERROR, functionName, "               feedrate limit", feedrate_limit);
        printNextError_ = false;
      }
    } else {
      status = setIntegerParam(this->PMAC_C_FeedRateProblem_, PMAC_OK_);
      printNextError_ = true;
    }
  }

  callParamCallbacks();

  if (status != asynSuccess){
    debug(DEBUG_ERROR, functionName, "Error reading or setting params.");
    setIntegerParam(PMAC_C_CommsError_, PMAC_ERROR_);
  } else {
    setIntegerParam(PMAC_C_CommsError_, PMAC_OK_);
  }

  return status;
}

asynStatus pmacController::updateStatistics()
{
  asynStatus status = asynSuccess;
  int noOfMsgs = 0;
  int totalBytesWritten = 0;
  int totalBytesRead = 0;
  int totalMsgTime = 0;
  int lastMsgBytesWritten = 0;
  int lastMsgBytesRead = 0;
  int lastMsgTime = 0;
  int maxBytesWritten = 0;
  int maxBytesRead = 0;
  int maxTime = 0;
  static const char *functionName = "updateStatistics";

  debug(DEBUG_FLOW, functionName);

  status = pBroker_->readStatistics(&noOfMsgs,
                                    &totalBytesWritten,
                                    &totalBytesRead,
                                    &totalMsgTime,
                                    &lastMsgBytesWritten,
                                    &lastMsgBytesRead,
                                    &lastMsgTime);

  setIntegerParam(PMAC_C_NoOfMsgs_, noOfMsgs);
  setIntegerParam(PMAC_C_TotalBytesWritten_, totalBytesWritten);
  setIntegerParam(PMAC_C_TotalBytesRead_, totalBytesRead);
  setIntegerParam(PMAC_C_MsgBytesWritten_, lastMsgBytesWritten);
  setIntegerParam(PMAC_C_MsgBytesRead_, lastMsgBytesRead);
  setIntegerParam(PMAC_C_MsgTime_, lastMsgTime);
  getIntegerParam(PMAC_C_MaxBytesWritten_, &maxBytesWritten);
  if (lastMsgBytesWritten > maxBytesWritten){
    setIntegerParam(PMAC_C_MaxBytesWritten_, lastMsgBytesWritten);
  }
  getIntegerParam(PMAC_C_MaxBytesRead_, &maxBytesRead);
  if (lastMsgBytesRead > maxBytesRead){
    setIntegerParam(PMAC_C_MaxBytesRead_, lastMsgBytesRead);
  }
  getIntegerParam(PMAC_C_MaxTime_, &maxTime);
  if (lastMsgTime > maxTime){
    setIntegerParam(PMAC_C_MaxTime_, lastMsgTime);
  }
  if (noOfMsgs > 0){
    setIntegerParam(PMAC_C_AveBytesWritten_, totalBytesWritten/noOfMsgs);
    setIntegerParam(PMAC_C_AveBytesRead_, totalBytesRead/noOfMsgs);
    setIntegerParam(PMAC_C_AveTime_, totalMsgTime/noOfMsgs);
  }
  callParamCallbacks();

  return status;
}

/**
 * Read the PMAC global status integer (using a ??? ) and global feed rate (%)
 * @param int The global status integer (23 active bits)
 * @param int The global feed rate
 */
/*
asynStatus pmacController::getGlobalStatus(epicsUInt32 *globalStatus, int *feedrate, int feedrate_poll)
{
  char command[PMAC_MAXBUF_];
  char response[PMAC_MAXBUF_];
  int nvals = 0;
  asynStatus status = asynSuccess;
  static const char *functionName = "pmacController::getGlobalStatus";

  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s\n", functionName);
  
  sprintf(command, "???");
  if (lowLevelWriteRead(command, response) != asynSuccess) {
    asynPrint(lowLevelPortUser_, ASYN_TRACE_ERROR, "%s: Error reading ???.\n", functionName);
    status = asynError;
  } else {
    nvals = sscanf(response, "%6x", globalStatus);
    if (nvals != 1) {
      asynPrint(lowLevelPortUser_, ASYN_TRACE_ERROR, "%s: Error reading ???. nvals: %d, response: %s\n", functionName, nvals, response);
      status = asynError;
    } else {
      status = asynSuccess;
    }
  }

  if (feedrate_poll) {
    sprintf(command, "%%");
    if (lowLevelWriteRead(command, response) != asynSuccess) {
      asynPrint(lowLevelPortUser_, ASYN_TRACE_ERROR, "%s: Error reading feedrate.\n", functionName);
      status = asynError;
    } else {
      nvals = sscanf(response, "%d", feedrate);
      if (nvals != 1) {
	asynPrint(lowLevelPortUser_, ASYN_TRACE_ERROR, "%s: Error reading feedrate: nvals: %d, response: %s\n", functionName, nvals, response);
	status = asynError;
      } else {
	status = asynSuccess;
      }
    }
  }
  
  if (status == asynSuccess) {
    setIntegerParam(PMAC_C_CommsError_, PMAC_OK_);
  } else {
    setIntegerParam(PMAC_C_CommsError_, PMAC_ERROR_);
  }

  return status;

}
*/

/**
 * Disable the check in the axis poller that reads ix24 to check if hardware limits
 * are disabled. By default this is enabled for safety reasons. It sets the motor
 * record PROBLEM bit in MSTA, which results in the record going into MAJOR/STATE alarm.
 * @param axis Axis number to disable the check for.
 */
asynStatus pmacController::pmacDisableLimitsCheck(int axis) 
{
  pmacAxis *pA = NULL;
  static const char *functionName = "pmacController::pmacDisableLimitsCheck";

  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s\n", functionName);

  this->lock();
  pA = getAxis(axis);
  if (pA) {
    pA->limitsCheckDisable_ = 1;
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
              "%s. Disabling hardware limits disable check on controller %s, axis %d\n", 
              functionName, portName, pA->axisNo_);
  } else {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
	      "%s: Error: axis %d has not been configured using pmacCreateAxis.\n", functionName, axis);
    return asynError;
  }
  this->unlock();
  return asynSuccess;
}

/**
 * Disable the check in the axis poller that reads ix24 to check if hardware limits
 * are disabled. By default this is enabled for safety reasons. It sets the motor
 * record PROBLEM bit in MSTA, which results in the record going into MAJOR/STATE alarm.
 * This function will disable the check for all axes on this controller.
 */
asynStatus pmacController::pmacDisableLimitsCheck(void) 
{
  pmacAxis *pA = NULL;
  static const char *functionName = "pmacController::pmacDisableLimitsCheck";

  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s\n", functionName);

  this->lock();
  for (int i=1; i<numAxes_; i++) {
    pA = getAxis(i);
    if (!pA) continue;
    pA->limitsCheckDisable_ = 1;
//    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
//              "%s. Disabling hardware limits disable check on controller %s, axis %d\n",
//              functionName, portName, pA->axisNo_);
  }
  this->unlock();
  return asynSuccess;
}


/**
 * Set the PMAC axis scale factor to increase resolution in the motor record.
 * Default value is 1.
 * @param axis Axis number to set the PMAC axis scale factor.
 * @param scale Scale factor to set
 */
asynStatus pmacController::pmacSetAxisScale(int axis, int scale) 
{
  pmacAxis *pA = NULL;
  static const char *functionName = "pmacController::pmacSetAxisScale";

  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s\n", functionName);

  if (scale < 1) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s: Error: scale factor must be >=1.\n", functionName);
    return asynError;
  }

  this->lock();
  pA = getAxis(axis);
  if (pA) {
    pA->scale_ = scale;
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
              "%s. Setting scale factor of %d on axis %d, on controller %s.\n",
              functionName, pA->scale_, pA->axisNo_, portName);

  } else {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
	      "%s: Error: axis %d has not been configured using pmacCreateAxis.\n", functionName, axis);
    return asynError;
  }
  this->unlock();
  return asynSuccess;
}


/**
 * If we have an open loop axis that has an encoder coming back on a different channel
 * then the encoder readback axis number can be set here. This ensures that the encoder
 * will be used for the position readback. It will also ensure that the encoder axis
 * is set correctly when performing a set position on the open loop axis.
 *
 * To use this function, the axis number used for the encoder must have been configured
 * already using pmacCreateAxis.
 *
 * @param controller The Asyn port name for the PMAC controller.
 * @param axis Axis number to set the PMAC axis scale factor.
 * @param encoder_axis The axis number that the encoder is fed into.  
 */
asynStatus pmacController::pmacSetOpenLoopEncoderAxis(int axis, int encoder_axis)
{
  pmacAxis *pA = NULL;
  static const char *functionName = "pmacController::pmacSetOpenLoopEncoderAxis";

  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s\n", functionName);

  this->lock();
  pA = getAxis(axis);
  if (pA) {
    //Test that the encoder axis has also been configured
    if (getAxis(encoder_axis) == NULL) {
      asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
		"%s: Error: encoder axis %d has not been configured using pmacCreateAxis.\n", functionName, encoder_axis);
      return asynError;
    }
    pA->encoder_axis_ = encoder_axis;
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
              "%s. Setting encoder axis %d for axis %d, on controller %s.\n",
              functionName, pA->encoder_axis_, pA->axisNo_, portName);

  } else {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
	      "%s: Error: axis %d has not been configured using pmacCreateAxis.\n", functionName, axis);
    return asynError;
  }
  this->unlock();
  return asynSuccess;
}

asynStatus pmacController::registerForCallbacks(pmacCallbackInterface *cbPtr, int type)
{
  return pBroker_->registerForUpdates(cbPtr, type);
}

asynStatus pmacController::monitorPMACVariable(int poll_speed, const char *var)
{
  return pBroker_->addReadVariable(poll_speed, var);
}

asynStatus pmacController::registerCS(pmacCSController *csPtr, int csNo)
{
  char statVar[8];
  static const char *functionName = "registerCS";

  debug(DEBUG_VARIABLE, functionName, "Registering CS", csNo);

  // Add the CS to the list
  pCSControllers_[csNo] = csPtr;
  pAxisZero->registerCS(csPtr, csNo);

  // First add the CS status item to the fast update
  sprintf(statVar, "&%d??", csNo);
  this->pBroker_->addReadVariable(pmacMessageBroker::PMAC_FAST_READ, statVar);

  // Now register the CS object for callbacks from the broker
  this->pBroker_->registerForUpdates(csPtr, pmacMessageBroker::PMAC_FAST_READ);

  return asynSuccess;
}

asynStatus pmacController::listPLCProgram(int plcNo, char *buffer, size_t size)
{
  int word = 0;
  char reply[PMAC_MAXBUF];
  char cmd[PMAC_MAXBUF];
  char line[PMAC_MAXBUF];
  int cword = 0;
  int running = 1;
  asynStatus status = asynSuccess;
  static const char *functionName = "listPLCProgram";

  startTimer(DEBUG_ERROR, functionName);
  debug(DEBUG_TRACE, functionName);
  debug(DEBUG_VARIABLE, functionName, "Listing PLC", plcNo);

  // Setup the list command
  strcpy(buffer, "");
  while (running == 1 && word < 10000){
    sprintf(cmd, "list plc%d,%d,1", plcNo, word);
    pBroker_->immediateWriteRead(cmd, reply);
    if (reply[0] == 0x7){
      running = 0;
    } else {
      sscanf(reply, "%d:%s", &cword, line);
      if (cword == word){
        if (strlen(buffer) + strlen(line) + 1 > size){
          // We cannot add the next line as the buffer would be full
          // Report the error
          running = 0;
          status = asynError;
        } else {
          strcat(buffer, line);
          strcat(buffer, " ");
        }
      }
    }
    word ++;
  }
  debug(DEBUG_VARIABLE, functionName, "PLC", buffer);
  stopTimer(DEBUG_ERROR, functionName, "Time taken to list PLC");

  return status;
}

asynStatus pmacController::storeKinematics()
{
  asynStatus status = asynSuccess;
  int csNo = 0;
  char buffer[20000];
  static const char *functionName = "storeKinematics";

  startTimer(DEBUG_TIMING, functionName);
  debug(DEBUG_TRACE, functionName);
  while (csNo < PMAC_MAX_CS && status == asynSuccess){
    // Read each forward kinematic
    status = listKinematic(csNo+1, "forward", buffer, sizeof(buffer));
    // Store into the appropriate parameter
    setStringParam(PMAC_C_ForwardKinematic_[csNo], buffer);
    // Read each inverse kinematic
    status = listKinematic(csNo+1, "inverse", buffer, sizeof(buffer));
    // Store into the appropriate parameter
    setStringParam(PMAC_C_InverseKinematic_[csNo], buffer);
    // Next coordinate system
    csNo++;
  }
  callParamCallbacks();
  if (status != asynSuccess){
    debug(DEBUG_ERROR, functionName, "Failed to read all Kinematics");
  }
  stopTimer(DEBUG_TIMING, functionName, "Time taken to store kinematics");
  return status;
}

asynStatus pmacController::listKinematic(int csNo, const std::string& type, char *buffer, size_t size)
{
  int word = 0;
  char reply[PMAC_MAXBUF];
  char cmd[PMAC_MAXBUF];
  char line[PMAC_MAXBUF];
  int cword = 0;
  int running = 1;
  asynStatus status = asynSuccess;
  static const char *functionName = "listKinematic";

  startTimer(DEBUG_TIMING, functionName);
  debug(DEBUG_TRACE, functionName);
  debug(DEBUG_VARIABLE, functionName, "Listing kinematics for CS", csNo);

  if (type != "forward" && type != "inverse"){
    status = asynError;
    debug(DEBUG_ERROR, functionName, "Unknown kinematic type", type);
  }

  if (status == asynSuccess){
    // Setup the list command
    // List by 1 word at a time, throw away duplicates
    // This is very inefficient, but currently necessary due
    // to the PMAC driver
    strcpy(buffer, "");
    while (running == 1 && word < 10000){
      sprintf(cmd, "&%d list %s,%d,1", csNo, type.c_str(), word);
      pBroker_->immediateWriteRead(cmd, reply);
      if (reply[0] == 0x7){
        running = 0;
      } else {
        sscanf(reply, "%d:%s", &cword, line);
        if (cword == word){
          if (strlen(buffer) + strlen(line) + 1 > size){
            // We cannot add the next line as the buffer would be full
            // Report the error
            running = 0;
            status = asynError;
            debug(DEBUG_ERROR, functionName, "Buffer not large enough for kinematic", (int)size);
          } else {
            strcat(buffer, line);
            strcat(buffer, " ");
          }
        }
      }
      word ++;
    }
    debug(DEBUG_VARIABLE, functionName, "Kinematic", buffer);
  }
  stopTimer(DEBUG_TIMING, functionName, "Time taken to list kinematic");

  return status;
}


asynStatus pmacController::processDeferredMoves(void)
{
  asynStatus status = asynSuccess;
  char command[PMAC_MAXBUF_] = {0};
  char response[PMAC_MAXBUF_] = {0};
  pmacAxis *pAxis = NULL;
  static const char *functionName = "pmacController::processDeferredMoves";

  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s\n", functionName);

  //Build up combined move command for all axes involved in the deferred move.
  for (int axis=0; axis<numAxes_; axis++) {
    pAxis = getAxis(axis);
    if (pAxis != NULL) {
      if (pAxis->deferredMove_) {
	sprintf(command, "%s #%d%s%.2f", command, pAxis->axisNo_,
		pAxis->deferredRelative_ ? "J^" : "J=",
		pAxis->deferredPosition_);
      }
    }
  }
  
  //Execute the deferred move
  if (lowLevelWriteRead(command, response) != asynSuccess) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s ERROR Sending Deferred Move Command.\n", functionName);
    setIntegerParam(PMAC_C_CommsError_, PMAC_ERROR_);
    status = asynError;
  } else {
    setIntegerParam(PMAC_C_CommsError_, PMAC_OK_);
    status = asynSuccess;
  }

  //Clear deferred move flag for the axes involved.
  for (int axis=0; axis<numAxes_; axis++) {
    pAxis = getAxis(axis);
    if (pAxis!=NULL) {
      if (pAxis->deferredMove_) {
	pAxis->deferredMove_ = 0;
      }
    }
  }
     
  return status;
}





/*************************************************************************************/
/** The following functions have C linkage, and can be called directly or from iocsh */

extern "C" {

/**
 * C wrapper for the pmacController constructor.
 * See pmacController::pmacController.
 *
 */
asynStatus pmacCreateController(const char *portName, const char *lowLevelPortName, int lowLevelPortAddress, 
				int numAxes, int movingPollPeriod, int idlePollPeriod)
{
  pmacController *ppmacController = new pmacController(portName, lowLevelPortName, lowLevelPortAddress, numAxes, movingPollPeriod/1000., idlePollPeriod/1000.);
  ppmacController->startPMACPolling();
  ppmacController = NULL;
  return asynSuccess;
}

/**
 * C wrapper for the pmacAxis constructor.
 * See pmacAxis::pmacAxis.
 *
 */
asynStatus pmacCreateAxis(const char *pmacName,         /* specify which controller by port name */
			  int axis)                    /* axis number (start from 1). */
{
  pmacController *pC;
  pmacAxis *pAxis;

  static const char *functionName = "pmacCreateAxis";

  pC = (pmacController*) findAsynPortDriver(pmacName);
  if (!pC) {
    printf("%s::%s: ERROR Port %s Not Found.\n",
           driverName, functionName, pmacName);
    return asynError;
  }

  if (axis == 0) {
    printf("%s::%s: ERROR Axis Number 0 Not Allowed. This Asyn Address Is Reserved For Controller Specific Parameters.\n",
	   driverName, functionName);
    return asynError;
  }
  
  pC->lock();
  pAxis = new pmacAxis(pC, axis);
  pAxis = NULL;
  pC->unlock();
  return asynSuccess;
}

/**
 * C Wrapper function for pmacAxis constructor.
 * See pmacAxis::pmacAxis.
 * This function allows creation of multiple pmacAxis objects with axis numbers 1 to numAxes.
 * @param pmacName Asyn port name for the controller (const char *)
 * @param numAxes The number of axes to create, starting at 1.
 *
 */
asynStatus pmacCreateAxes(const char *pmacName,        
			  int numAxes)                   
{
  pmacController *pC;
  pmacAxis *pAxis;

  static const char *functionName = "pmacCreateAxis";

  pC = (pmacController*) findAsynPortDriver(pmacName);
  if (!pC) {
    printf("%s:%s: Error port %s not found\n",
           driverName, functionName, pmacName);
    return asynError;
  }
  
  pC->lock();
  for (int axis=1; axis<=numAxes; axis++) {
    pAxis = new pmacAxis(pC, axis);
    pAxis = NULL;
  }
  pC->unlock();
  return asynSuccess;
}


/**
 * Disable the check in the axis poller that reads ix24 to check if hardware limits
 * are disabled. By default this is enabled for safety reasons. It sets the motor
 * record PROBLEM bit in MSTA, which results in the record going into MAJOR/STATE alarm.
 * @param controller Asyn port name for the controller (const char *)
 * @param axis Axis number to disable the check for.
 * @param allAxes Set to 0 if only dealing with one axis. 
 *                Set to 1 to do all axes (in which case the axis parameter is ignored).
 */
asynStatus pmacDisableLimitsCheck(const char *controller, int axis, int allAxes)
{
  pmacController *pC;
  static const char *functionName = "pmacDisableLimitsCheck";

  pC = (pmacController*) findAsynPortDriver(controller);
  if (!pC) {
    printf("%s:%s: Error port %s not found\n",
           driverName, functionName, controller);
    return asynError;
  }

  if (allAxes == 1) {
    return pC->pmacDisableLimitsCheck();
  } else if (allAxes == 0) {
    return pC->pmacDisableLimitsCheck(axis);
  }

  return asynError;
}


/**
 * Set the PMAC axis scale factor to increase resolution in the motor record.
 * Default value is 1.
 * @param controller The Asyn port name for the PMAC controller.
 * @param axis Axis number to set the PMAC axis scale factor.
 * @param scale Scale factor to set
 */
asynStatus pmacSetAxisScale(const char *controller, int axis, int scale)
{
  pmacController *pC;
  static const char *functionName = "pmacSetAxisScale";

  pC = (pmacController*) findAsynPortDriver(controller);
  if (!pC) {
    printf("%s:%s: Error port %s not found\n",
           driverName, functionName, controller);
    return asynError;
  }
    
  return pC->pmacSetAxisScale(axis, scale);
}

  
/**
 * If we have an open loop axis that has an encoder coming back on a different channel
 * then the encoder readback axis number can be set here. This ensures that the encoder
 * will be used for the position readback. It will also ensure that the encoder axis
 * is set correctly when performing a set position on the open loop axis.
 *
 * To use this function, the axis number used for the encoder must have been configured
 * already using pmacCreateAxis.
 *
 * @param controller The Asyn port name for the PMAC controller.
 * @param axis Axis number to set the PMAC axis scale factor.
 * @param encoder_axis The axis number that the encoder is fed into.  
 */
asynStatus pmacSetOpenLoopEncoderAxis(const char *controller, int axis, int encoder_axis)
{
  pmacController *pC;
  static const char *functionName = "pmacSetOpenLoopEncoderAxis";

  pC = (pmacController*) findAsynPortDriver(controller);
  if (!pC) {
    printf("%s:%s: Error port %s not found\n",
           driverName, functionName, controller);
    return asynError;
  }
    
  return pC->pmacSetOpenLoopEncoderAxis(axis, encoder_axis);
}

/**
 * Creates a Coordinate System Group to allow us to switch axes into coordinate systems
 *
 * @param controller The Asyn port name for the PMAC controller.
 * @param group number for this CS group
 * @param number of axes in this CS group
 */
asynStatus pmacCreateCsGroup(const char *controller, int groupNo, char* groupName, int axisCount)
{
	pmacController *pC;
	static const char *functionName = "pmacCreateCsGroup";

	pC = (pmacController*) findAsynPortDriver(controller);
	if (!pC)
	{
		printf("%s:%s: Error port %s not found\n",
			   driverName, functionName, controller);
		return asynError;
	}

	pC->pGroupList->addGroup(groupNo, groupName, axisCount);

	return asynSuccess;
}

/**
 * Adds an axis to a Coordinate System Group
 *
 * @param controller The Asyn port name for the PMAC controller.
 * @param group number for this CS group
 * @param axis number to add
 * @param axis definition for this axis
 * @param coordinate system no. for this axis definition
 *
 */
asynStatus pmacCsGroupAddAxis(const char *controller, int groupNo, int axisNo, char *mapping, int coordinateSysNo)
{
  pmacController *pC;
  static const char *functionName = "pmacCsGroupAddAxis";

  pC = (pmacController*) findAsynPortDriver(controller);
  if (!pC)
  {
    printf("%s:%s: Error port %s not found\n",
         driverName, functionName, controller);
    return asynError;
  }

  pC->pGroupList->addAxisToGroup(groupNo, axisNo, mapping, coordinateSysNo);

  return asynSuccess;
}

/**
 * Sets the debug level for the PMAC controller or an axis
 *
 * @param controller The Asyn port name for the PMAC controller.
 * @param level The required level of debug (bitmask).
 * @param axis (0 for controller, 1-n for axis number).
 *
 */
asynStatus pmacDebug(const char *controller, int level, int axis)
{
  pmacController *pC;
  static const char *functionName = "pmacDebug";

  pC = (pmacController*) findAsynPortDriver(controller);
  if (!pC)
  {
    printf("%s:%s: Error port %s not found\n",
         driverName, functionName, controller);
    return asynError;
  }

  pC->setDebugLevel(level, axis);

  return asynSuccess;
}



/* Code for iocsh registration */

/* pmacCreateController */
static const iocshArg pmacCreateControllerArg0 = {"Controller port name", iocshArgString};
static const iocshArg pmacCreateControllerArg1 = {"Low level port name", iocshArgString};
static const iocshArg pmacCreateControllerArg2 = {"Low level port address", iocshArgInt};
static const iocshArg pmacCreateControllerArg3 = {"Number of axes", iocshArgInt};
static const iocshArg pmacCreateControllerArg4 = {"Moving poll rate (ms)", iocshArgInt};
static const iocshArg pmacCreateControllerArg5 = {"Idle poll rate (ms)", iocshArgInt};
static const iocshArg * const pmacCreateControllerArgs[] = {&pmacCreateControllerArg0,
							    &pmacCreateControllerArg1,
							    &pmacCreateControllerArg2,
							    &pmacCreateControllerArg3,
							    &pmacCreateControllerArg4,
							    &pmacCreateControllerArg5};
static const iocshFuncDef configpmacCreateController = {"pmacCreateController", 6, pmacCreateControllerArgs};
static void configpmacCreateControllerCallFunc(const iocshArgBuf *args)
{
  pmacCreateController(args[0].sval, args[1].sval, args[2].ival, args[3].ival, args[4].ival, args[5].ival);
}


/* pmacCreateAxis */
static const iocshArg pmacCreateAxisArg0 = {"Controller port name", iocshArgString};
static const iocshArg pmacCreateAxisArg1 = {"Axis number", iocshArgInt};
static const iocshArg * const pmacCreateAxisArgs[] = {&pmacCreateAxisArg0,
                                                     &pmacCreateAxisArg1};
static const iocshFuncDef configpmacAxis = {"pmacCreateAxis", 2, pmacCreateAxisArgs};

static void configpmacAxisCallFunc(const iocshArgBuf *args)
{
  pmacCreateAxis(args[0].sval, args[1].ival);
}

/* pmacCreateAxes */
static const iocshArg pmacCreateAxesArg0 = {"Controller port name", iocshArgString};
static const iocshArg pmacCreateAxesArg1 = {"Num Axes", iocshArgInt};
static const iocshArg * const pmacCreateAxesArgs[] = {&pmacCreateAxesArg0,
                                                     &pmacCreateAxesArg1};
static const iocshFuncDef configpmacAxes = {"pmacCreateAxes", 2, pmacCreateAxesArgs};

static void configpmacAxesCallFunc(const iocshArgBuf *args)
{
  pmacCreateAxes(args[0].sval, args[1].ival);
}


/* pmacDisableLimitsCheck */
static const iocshArg pmacDisableLimitsCheckArg0 = {"Controller port name", iocshArgString};
static const iocshArg pmacDisableLimitsCheckArg1 = {"Axis number", iocshArgInt};
static const iocshArg pmacDisableLimitsCheckArg2 = {"All Axes", iocshArgInt};
static const iocshArg * const pmacDisableLimitsCheckArgs[] = {&pmacDisableLimitsCheckArg0,
							      &pmacDisableLimitsCheckArg1,
							      &pmacDisableLimitsCheckArg2};
static const iocshFuncDef configpmacDisableLimitsCheck = {"pmacDisableLimitsCheck", 3, pmacDisableLimitsCheckArgs};

static void configpmacDisableLimitsCheckCallFunc(const iocshArgBuf *args)
{
  pmacDisableLimitsCheck(args[0].sval, args[1].ival, args[2].ival);
}



/* pmacSetAxisScale */
static const iocshArg pmacSetAxisScaleArg0 = {"Controller port name", iocshArgString};
static const iocshArg pmacSetAxisScaleArg1 = {"Axis number", iocshArgInt};
static const iocshArg pmacSetAxisScaleArg2 = {"Scale", iocshArgInt};
static const iocshArg * const pmacSetAxisScaleArgs[] = {&pmacSetAxisScaleArg0,
							      &pmacSetAxisScaleArg1,
							      &pmacSetAxisScaleArg2};
static const iocshFuncDef configpmacSetAxisScale = {"pmacSetAxisScale", 3, pmacSetAxisScaleArgs};

static void configpmacSetAxisScaleCallFunc(const iocshArgBuf *args)
{
  pmacSetAxisScale(args[0].sval, args[1].ival, args[2].ival);
}

/* pmacSetOpenLoopEncoderAxis */
static const iocshArg pmacSetOpenLoopEncoderAxisArg0 = {"Controller port name", iocshArgString};
static const iocshArg pmacSetOpenLoopEncoderAxisArg1 = {"Axis number", iocshArgInt};
static const iocshArg pmacSetOpenLoopEncoderAxisArg2 = {"Encoder Axis", iocshArgInt};
static const iocshArg * const pmacSetOpenLoopEncoderAxisArgs[] = {&pmacSetOpenLoopEncoderAxisArg0,
								  &pmacSetOpenLoopEncoderAxisArg1,
								  &pmacSetOpenLoopEncoderAxisArg2};
static const iocshFuncDef configpmacSetOpenLoopEncoderAxis = {"pmacSetOpenLoopEncoderAxis", 3, pmacSetOpenLoopEncoderAxisArgs};

static void configpmacSetOpenLoopEncoderAxisCallFunc(const iocshArgBuf *args)
{
  pmacSetOpenLoopEncoderAxis(args[0].sval, args[1].ival, args[2].ival);
}

/* pmacCreateCsGroup */
static const iocshArg pmacCreateCsGroupArg0 = {"Controller port name", iocshArgString};
static const iocshArg pmacCreateCsGroupArg1 = {"Group number", iocshArgInt};
static const iocshArg pmacCreateCsGroupArg2 = {"Group name", iocshArgString};
static const iocshArg pmacCreateCsGroupArg3 = {"Axis count", iocshArgInt};
static const iocshArg * const pmacCreateCsGroupArgs[] = {&pmacCreateCsGroupArg0,
								  &pmacCreateCsGroupArg1,
								  &pmacCreateCsGroupArg2,
								  &pmacCreateCsGroupArg3};
static const iocshFuncDef configpmacCreateCsGroup = {"pmacCreateCsGroup", 4, pmacCreateCsGroupArgs};

static void configpmacCreateCsGroupCallFunc(const iocshArgBuf *args)
{
	pmacCreateCsGroup(args[0].sval, args[1].ival, args[2].sval, args[3].ival);
}

/* pmacCsGroupAddAxis */
static const iocshArg pmacCsGroupAddAxisArg0 = {"Controller port name", iocshArgString};
static const iocshArg pmacCsGroupAddAxisArg1 = {"Group number", iocshArgInt};
static const iocshArg pmacCsGroupAddAxisArg2 = {"Axis number", iocshArgInt};
static const iocshArg pmacCsGroupAddAxisArg3 = {"Axis CS definition", iocshArgString};
static const iocshArg pmacCsGroupAddAxisArg4 = {"CS number", iocshArgInt};
static const iocshArg * const pmacCsGroupAddAxisArgs[] = {&pmacCsGroupAddAxisArg0,
                  &pmacCsGroupAddAxisArg1,
                  &pmacCsGroupAddAxisArg2,
                  &pmacCsGroupAddAxisArg3,
                  &pmacCsGroupAddAxisArg4};
static const iocshFuncDef configpmacCsGroupAddAxis = {"pmacCsGroupAddAxis", 5, pmacCsGroupAddAxisArgs};

static void configpmacCsGroupAddAxisCallFunc(const iocshArgBuf *args)
{
  pmacCsGroupAddAxis(args[0].sval, args[1].ival, args[2].ival, args[3].sval, args[4].ival);
}

/* pmacCsGroupAddAxis */
static const iocshArg pmacDebugArg0 = {"Controller port name", iocshArgString};
static const iocshArg pmacDebugArg1 = {"Debug level", iocshArgInt};
static const iocshArg pmacDebugArg2 = {"Axis number", iocshArgInt};
static const iocshArg * const pmacDebugArgs[] = {&pmacDebugArg0,
                                                 &pmacDebugArg1,
                                                 &pmacDebugArg2};
static const iocshFuncDef configpmacDebug = {"pmacDebug", 3, pmacDebugArgs};

static void configpmacDebugCallFunc(const iocshArgBuf *args)
{
  pmacDebug(args[0].sval, args[1].ival, args[2].ival);
}


static void pmacControllerRegister(void)
{
  iocshRegister(&configpmacCreateController,       configpmacCreateControllerCallFunc);
  iocshRegister(&configpmacAxis,                   configpmacAxisCallFunc);
  iocshRegister(&configpmacAxes,                   configpmacAxesCallFunc);
  iocshRegister(&configpmacDisableLimitsCheck,     configpmacDisableLimitsCheckCallFunc);
  iocshRegister(&configpmacSetAxisScale,           configpmacSetAxisScaleCallFunc);
  iocshRegister(&configpmacSetOpenLoopEncoderAxis, configpmacSetOpenLoopEncoderAxisCallFunc);
  iocshRegister(&configpmacCreateCsGroup,          configpmacCreateCsGroupCallFunc);
  iocshRegister(&configpmacCsGroupAddAxis,         configpmacCsGroupAddAxisCallFunc);
  iocshRegister(&configpmacDebug,                  configpmacDebugCallFunc);
}
epicsExportRegistrar(pmacControllerRegister);

#ifdef vxWorks
  //VxWorks register functions
  epicsRegisterFunction(pmacCreateController);
  epicsRegisterFunction(pmacCreateAxis);
  epicsRegisterFunction(pmacCreateAxes);
  epicsRegisterFunction(pmacDisableLimitsCheck);
  epicsRegisterFunction(pmacSetAxisScale);
  epicsRegisterFunction(pmacSetOpenLoopEncoderAxis);
  epicsRegisterFunction(pmacDebug);
#endif
} // extern "C"

