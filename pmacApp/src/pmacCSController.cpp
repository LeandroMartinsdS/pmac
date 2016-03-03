/*
 * pmacCSController.cpp
 *
 *  Created on: 24 Feb 2016
 *      Author: Alan Greer
 *
 *
\file

This is a driver for Delta-Tau PMAC Coordinate Systems,
designed to interface with the "motor API" style interface that
allows drivers to be used with the standard asyn device and driver
support for the motor record

To use this driver, you should create a co-ordinate system with kinematics and a
position reporting PLC as detailed in the
<a href="http://www.deltatau.com/manuals/pdfs/TURBO%20PMAC%20USER%20MANUAL.pdf">
Delta Tau Turbo PMAC User Manual</a>, along with a position reporting PLC. You
should follow the conventions set out in the
<a href="../BLS-GEN-CTRL-0005.doc">DLS PMAC Programming Standards</a>

The driver uses the following interface:
- CS Axes A, B, C, U, V, W, X, Y, Z are mapped to Q1..9 in kinematic code by the
PMAC
- The EPICS driver expects the position reporting PLC to report the positions of
these axes on Q81..89, in EGUs
- The driver applies a scale factor (10000.0 by default, modified by
pmacSetCoordStepsPerUnit). This is needed because the motor record can only
drive in integer "steps". The motor record needs to have the inverse of this
scale factor in MRES (0.0001 by default)
- The driver is numbered from 0, so the motor record with address 0 connects
to axis A which has readback in Q81
- When a move is demanded, the following is done:
 - The CS is aborted to put the motors in closed loop mode
 - Isx87 (Acceleration) is set to VELO / ACCL * 1000
 - Isx89 (Feedrate) is set to the motor record VELO
 - Demand value in EGUs is written to the relevant Q Variable in Q71..79
 - Motion program specified in the startup script (probably
PROG_10_CS_motion.pmc) is run.

The following code will open an IP Port to an ethernet PMAC, then wrap CS2
in a driver with ref 0 running prog 10, polling for position every 0.5s
when the motors aren't moving, and every 0.1s when they are, and set the scale
factor for X on CS2 to 100000.0, so the relevant motor record will need an MRES
of 1.0/100000.0
\code
# Create IP Port (IPPort, IPAddr)
# This function lives in tpmac
pmacAsynIPConfigure("BRICK1port", "172.23.243.156:1025")

# Create CS (ControllerPort, Addr, CSNumber, CSRef, Prog)
pmacAsynCoordCreate("BRICK1port", 0, 2, 0, 10)
# Configure CS (PortName, DriverName, CSRef, NAxes)
drvAsynMotorConfigure("BRICK1CS2", "pmacAsynCoord", 0, 9)
# Set scale factor (CS_Ref, axis, stepsPerUnit)
pmacSetCoordStepsPerUnit(0, 6, 100000.0)
# Set Idle and Moving poll periods (CS_Ref, PeriodMilliSeconds)
pmacSetCoordIdlePollPeriod(0, 500)
pmacSetCoordMovingPollPeriod(0, 100)
\endcode
 */

#include <iocsh.h>
#include <drvSup.h>
#include <registryFunction.h>
#include <epicsExport.h>
#include "pmacCSController.h"
#include "pmacController.h"

const epicsUInt32 pmacCSController::PMAC_ERROR_PRINT_TIME_ = 600; //seconds

const epicsUInt32 pmacCSController::CS_STATUS1_RUNNING_PROG        = (0x1<<0);
const epicsUInt32 pmacCSController::CS_STATUS1_SINGLE_STEP_MODE    = (0x1<<1);
const epicsUInt32 pmacCSController::CS_STATUS1_CONTINUOUS_MODE     = (0x1<<2);
const epicsUInt32 pmacCSController::CS_STATUS1_MOVE_BY_TIME_MODE   = (0x1<<3);
const epicsUInt32 pmacCSController::CS_STATUS1_CONTINUOUS_REQUEST  = (0x1<<4);
const epicsUInt32 pmacCSController::CS_STATUS1_RADIUS_INC_MODE     = (0x1<<5);
const epicsUInt32 pmacCSController::CS_STATUS1_A_INC               = (0x1<<6);
const epicsUInt32 pmacCSController::CS_STATUS1_A_FEEDRATE          = (0x1<<7);
const epicsUInt32 pmacCSController::CS_STATUS1_B_INC               = (0x1<<8);
const epicsUInt32 pmacCSController::CS_STATUS1_B_FEEDRATE          = (0x1<<9);
const epicsUInt32 pmacCSController::CS_STATUS1_C_INC               = (0x1<<10);
const epicsUInt32 pmacCSController::CS_STATUS1_C_FEEDRATE          = (0x1<<11);
const epicsUInt32 pmacCSController::CS_STATUS1_U_INC               = (0x1<<12);
const epicsUInt32 pmacCSController::CS_STATUS1_U_FEEDRATE          = (0x1<<13);
const epicsUInt32 pmacCSController::CS_STATUS1_V_INC               = (0x1<<14);
const epicsUInt32 pmacCSController::CS_STATUS1_V_FEEDRATE          = (0x1<<15);
const epicsUInt32 pmacCSController::CS_STATUS1_W_INC               = (0x1<<16);
const epicsUInt32 pmacCSController::CS_STATUS1_W_FEEDRATE          = (0x1<<17);
const epicsUInt32 pmacCSController::CS_STATUS1_X_INC               = (0x1<<18);
const epicsUInt32 pmacCSController::CS_STATUS1_X_FEEDRATE          = (0x1<<19);
const epicsUInt32 pmacCSController::CS_STATUS1_Y_INC               = (0x1<<20);
const epicsUInt32 pmacCSController::CS_STATUS1_Y_FEEDRATE          = (0x1<<21);
const epicsUInt32 pmacCSController::CS_STATUS1_Z_INC               = (0x1<<22);
const epicsUInt32 pmacCSController::CS_STATUS1_Z_FEEDRATE          = (0x1<<23);

const epicsUInt32 pmacCSController::CS_STATUS2_CIRCLE_SPLINE_MODE  = (0x1<<0);
const epicsUInt32 pmacCSController::CS_STATUS2_CCW_RAPID_MODE      = (0x1<<1);
const epicsUInt32 pmacCSController::CS_STATUS2_2D_CUTTER_COMP      = (0x1<<2);
const epicsUInt32 pmacCSController::CS_STATUS2_2D_LEFT_3D_CUTTER   = (0x1<<3);
const epicsUInt32 pmacCSController::CS_STATUS2_PVT_SPLINE_MODE     = (0x1<<4);
const epicsUInt32 pmacCSController::CS_STATUS2_SEG_STOPPING        = (0x1<<5);
const epicsUInt32 pmacCSController::CS_STATUS2_SEG_ACCEL           = (0x1<<6);
const epicsUInt32 pmacCSController::CS_STATUS2_SEG_MOVING          = (0x1<<7);
const epicsUInt32 pmacCSController::CS_STATUS2_PRE_JOG             = (0x1<<8);
const epicsUInt32 pmacCSController::CS_STATUS2_CUTTER_MOVE_BUFFD   = (0x1<<9);
const epicsUInt32 pmacCSController::CS_STATUS2_CUTTER_STOP         = (0x1<<10);
const epicsUInt32 pmacCSController::CS_STATUS2_CUTTER_COMP_OUTSIDE = (0x1<<11);
const epicsUInt32 pmacCSController::CS_STATUS2_DWELL_MOVE_BUFFD    = (0x1<<12);
const epicsUInt32 pmacCSController::CS_STATUS2_SYNCH_M_ONESHOT     = (0x1<<13);
const epicsUInt32 pmacCSController::CS_STATUS2_EOB_STOP            = (0x1<<14);
const epicsUInt32 pmacCSController::CS_STATUS2_DELAYED_CALC        = (0x1<<15);
const epicsUInt32 pmacCSController::CS_STATUS2_ROTARY_BUFF         = (0x1<<16);
const epicsUInt32 pmacCSController::CS_STATUS2_IN_POSITION         = (0x1<<17);
const epicsUInt32 pmacCSController::CS_STATUS2_FOLLOW_WARN         = (0x1<<18);
const epicsUInt32 pmacCSController::CS_STATUS2_FOLLOW_ERR          = (0x1<<19);
const epicsUInt32 pmacCSController::CS_STATUS2_AMP_FAULT           = (0x1<<20);
const epicsUInt32 pmacCSController::CS_STATUS2_MOVE_IN_STACK       = (0x1<<21);
const epicsUInt32 pmacCSController::CS_STATUS2_RUNTIME_ERR         = (0x1<<22);
const epicsUInt32 pmacCSController::CS_STATUS2_LOOKAHEAD           = (0x1<<23);

const epicsUInt32 pmacCSController::CS_STATUS3_LIMIT               = (0x1<<1);

/**
 * Create a driver instance to communicate with a given coordinate system
 *
 * @param portName The Asyn port used to communicate with the PMAC card
 * @param controllerPortName
 * @param addr The Asyn address of the PMAC (usually 0)
 * @param csNo The co-ordinate system to connect to
 * @param ref A unique reference, used by the higher layer software to reference this C.S.
 * @param program The PMAC program number to run to move the C.S.
 */
pmacCSController::pmacCSController(const char *portName, const char *controllerPortName, int csNo, int program)
  : asynMotorController(portName, 10, NUM_MOTOR_DRIVER_PARAMS+NUM_PMAC_CS_PARAMS,
    0, // No additional interfaces
    0, // No addition interrupt interfaces
    ASYN_CANBLOCK | ASYN_MULTIDEVICE,
    1, // autoconnect
    0, 0),  // Default priority and stack size
  pmacDebugger("pmacCSController"),
  csNumber_(csNo),
  progNumber_(program)
{
  asynStatus status = asynSuccess;
  static const char *functionName = "pmacCSController";

  pAxes_ = (pmacCSAxis **)(asynMotorController::pAxes_);

  pC_ = findAsynPortDriver(controllerPortName);
  if (!pC_) {
    debug(DEBUG_ERROR,functionName, "ERROR port not found", controllerPortName);
    status = asynError;
  }

  // Registration with the main controller. Register this coordinate system
  if (status == asynSuccess){
    ((pmacController *)pC_)->registerCS(this, csNumber_);
  }
}

pmacCSController::~pmacCSController()
{

}

bool pmacCSController::getMoving()
{
  pmacCSAxis *pA = NULL;
  bool anyMoving = false;
  static const char *functionName = "getMoving";
  debug(DEBUG_TRACE, functionName, "Called");

  this->lock();
  for (int i=0; i<numAxes_; i++) {
    pA = getAxis(i);
    if (!pA) continue;
    anyMoving |= pA->getMoving();
  }
  this->unlock();
  debug(DEBUG_VARIABLE, functionName, "Any axes moving", (int)anyMoving);
  return anyMoving;
}

int pmacCSController::getCSNumber()
{
  return csNumber_;
}

int pmacCSController::getProgramNumber()
{
  return progNumber_;
}

void pmacCSController::callback(pmacCommandStore *sPtr, int type)
{
  static const char *functionName = "callback";

  debug(DEBUG_TRACE, functionName, "Coordinate system status callback");
}

asynStatus pmacCSController::immediateWriteRead(const char *command, char *response)
{
  static const char *functionName = "immediateWriteRead";
  asynStatus status = asynSuccess;

  if (!pC_){
    debug(DEBUG_ERROR,functionName, "ERROR PMAC controller not found");
    status = asynError;
  }

  // Send the write/read demand to the PMAC controller
  if (status == asynSuccess){
    ((pmacController *)pC_)->immediateWriteRead(command, response);
  }
}

/** Returns a pointer to an pmacAxis object.
  * Returns NULL if the axis number is invalid.
  * \param[in] axisNo Axis index number. */
pmacCSAxis *pmacCSController::getAxis(int axisNo)
{
  if ((axisNo < 0) || (axisNo >= numAxes_)) return NULL;
  return pAxes_[axisNo];
}

// Registration for callbacks
asynStatus pmacCSController::registerForCallbacks(pmacCallbackInterface *cbPtr, int type)
{
  // Simply forward the request to the main controller
  return ((pmacController *)pC_)->registerForCallbacks(cbPtr, type);
}


asynStatus pmacCSController::monitorPMACVariable(int poll_speed, const char *var)
{
  // Simply forward the request to the main controller
  return ((pmacController *)pC_)->monitorPMACVariable(poll_speed, var);
}

/*************************************************************************************/
/** The following functions have C linkage, and can be called directly or from iocsh */

extern "C" {

/**
 * C wrapper for the pmacCSController constructor.
 * See pmacCSController::pmacCSController.
 *
 */
asynStatus pmacCSCreateController(const char *portName,
                                  const char *controllerPortName,
                                  int csNo,
                                  int program)
{
    pmacCSController *ptrpmacCSController = new pmacCSController(portName,
                                                                 controllerPortName,
                                                                 csNo,
                                                                 program);
    ptrpmacCSController = NULL;

    return asynSuccess;
}

/**
 * C wrapper for the pmacCSAxis constructor.
 * See pmacCSAxis::pmacCSAxis.
 *
 */
asynStatus pmacCreateCSAxis(const char *pmacName, /* specify which controller by port name */
                            int axis)             /* axis number (start from 1). */
{
  pmacCSController *pC;
  pmacCSAxis *pAxis;

  static const char *functionName = "pmacCreateCSAxis";

  pC = (pmacCSController *)findAsynPortDriver(pmacName);
  if (!pC) {
    printf("%s: ERROR Port %s Not Found.\n", functionName, pmacName);
    return asynError;
  }

  if (axis == 0) {
    printf("%s: ERROR Axis Number 0 Not Allowed. This Asyn Address Is Reserved For Controller Specific Parameters.\n", functionName);
    return asynError;
  }

  pC->lock();
  pAxis = new pmacCSAxis(pC, axis);
  pAxis = NULL;
  pC->unlock();
  return asynSuccess;
}

/**
 * C Wrapper function for pmacAxis constructor.
 * See pmacCSAxis::pmacCSAxis.
 * This function allows creation of multiple pmacAxis objects with axis numbers 1 to numAxes.
 * @param pmacName Asyn port name for the controller (const char *)
 * @param numAxes The number of axes to create, starting at 1.
 *
 */
asynStatus pmacCreateCSAxes(const char *pmacName, /* specify which controller by port name */
                            int numAxes)          /* Number of axes to create */
{
  pmacCSController *pC;
  pmacCSAxis *pAxis;

  static const char *functionName = "pmacCreateCSAxis";

  pC = (pmacCSController *)findAsynPortDriver(pmacName);
  if (!pC) {
    printf("%s: Error port %s not found\n", functionName, pmacName);
    return asynError;
  }

  pC->lock();
  for (int axis=1; axis<=numAxes; axis++) {
    pAxis = new pmacCSAxis(pC, axis);
    pAxis = NULL;
  }
  pC->unlock();
  return asynSuccess;
}

/* Code for iocsh registration */

/* pmacCreateController */
static const iocshArg pmacCreateCSControllerArg0 = {"Coordinate system port name", iocshArgString};
static const iocshArg pmacCreateCSControllerArg1 = {"PMAC controller port name", iocshArgString};
static const iocshArg pmacCreateCSControllerArg2 = {"Coordinate system number", iocshArgInt};
static const iocshArg pmacCreateCSControllerArg3 = {"Motion program execution number", iocshArgInt};
static const iocshArg * const pmacCreateCSControllerArgs[] = {&pmacCreateCSControllerArg0,
                  &pmacCreateCSControllerArg1,
                  &pmacCreateCSControllerArg2,
                  &pmacCreateCSControllerArg3};
static const iocshFuncDef configpmacCreateController = {"pmacCreateCS", 4, pmacCreateCSControllerArgs};
static void configpmacCreateCSControllerCallFunc(const iocshArgBuf *args)
{
  pmacCSCreateController(args[0].sval, args[1].sval, args[2].ival, args[3].ival);
}

/* pmacCreateCSAxis */
static const iocshArg pmacCreateCSAxisArg0 = {"Controller port name", iocshArgString};
static const iocshArg pmacCreateCSAxisArg1 = {"Axis number", iocshArgInt};
static const iocshArg * const pmacCreateCSAxisArgs[] = {&pmacCreateCSAxisArg0,
                                                        &pmacCreateCSAxisArg1};
static const iocshFuncDef configpmacCSAxis = {"pmacCreateCSAxis", 2, pmacCreateCSAxisArgs};

static void configpmacCSAxisCallFunc(const iocshArgBuf *args)
{
  pmacCreateCSAxis(args[0].sval, args[1].ival);
}

/* pmacCreateAxes */
static const iocshArg pmacCreateCSAxesArg0 = {"Controller port name", iocshArgString};
static const iocshArg pmacCreateCSAxesArg1 = {"Num Axes", iocshArgInt};
static const iocshArg * const pmacCreateCSAxesArgs[] = {&pmacCreateCSAxesArg0,
                                                        &pmacCreateCSAxesArg1};
static const iocshFuncDef configpmacCSAxes = {"pmacCreateCSAxes", 2, pmacCreateCSAxesArgs};

static void configpmacCSAxesCallFunc(const iocshArgBuf *args)
{
  pmacCreateCSAxes(args[0].sval, args[1].ival);
}

static void pmacCSControllerRegister(void)
{
  iocshRegister(&configpmacCreateController,   configpmacCreateCSControllerCallFunc);
  iocshRegister(&configpmacCSAxis,             configpmacCSAxisCallFunc);
  iocshRegister(&configpmacCSAxes,             configpmacCSAxesCallFunc);
}
epicsExportRegistrar(pmacCSControllerRegister);

#ifdef vxWorks
  //VxWorks register functions
epicsRegisterFunction(pmacCSCreateController);
epicsRegisterFunction(pmacCreateCSAxis);
epicsRegisterFunction(pmacCreateCSAxes);
#endif
} // extern "C"



