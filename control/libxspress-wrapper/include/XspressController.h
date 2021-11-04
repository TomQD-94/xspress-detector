/*
 * XspressController.h
 *
 *  Created on: 22 Sep 2021
 *      Author: Diamond Light Source
 */

#ifndef XspressController_H_
#define XspressController_H_

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/helpers/exception.h>

#include "logging.h"
#include "IpcReactor.h"
#include "IpcChannel.h"
#include "IpcMessage.h"
#include "XspressDetector.h"

using namespace log4cxx;
using namespace log4cxx::helpers;


namespace Xspress
{

/**
 * The XspressController class has overall responsibility for management of the
 * core and wrapper classes present within the XspressController application.
 * The class is responsible for coordinating control messages into libxpress and
 * handling the data arriving out from the library.
 *
 * The class uses an IpcReactor to manage connections and status updates.
 */
class XspressController : public boost::enable_shared_from_this<XspressController>
{
public:
  XspressController();
  virtual ~XspressController();
  void handleCtrlChannel();
  void provideStatus(OdinData::IpcMessage& reply);
  void provideVersion(OdinData::IpcMessage& reply);
  void configure(OdinData::IpcMessage& config, OdinData::IpcMessage& reply);
  void configureXsp(OdinData::IpcMessage& config, OdinData::IpcMessage& reply);
  void configureCommand(OdinData::IpcMessage& config, OdinData::IpcMessage& reply);
  void requestConfiguration(OdinData::IpcMessage& reply);
  void resetStatistics(OdinData::IpcMessage& reply);
  void run();
  void waitForShutdown();
  void shutdown();

private:
  /** Configuration constant to shutdown the xspress controller **/
  static const std::string CONFIG_SHUTDOWN;

  /** Configuration constant to set the debug level of the xspress controller **/
  static const std::string CONFIG_DEBUG;

  /** Configuration constant for control socket endpoint **/
  static const std::string CONFIG_CTRL_ENDPOINT;

  /** Configuration constants for parameters **/
  static const std::string CONFIG_XSP;
  static const std::string CONFIG_XSP_NUM_CARDS;
  static const std::string CONFIG_XSP_NUM_TF;
  static const std::string CONFIG_XSP_BASE_IP;
  static const std::string CONFIG_XSP_MAX_CHANNELS;
  static const std::string CONFIG_XSP_MAX_SPECTRA;
  static const std::string CONFIG_XSP_DEBUG;
  static const std::string CONFIG_XSP_CONFIG_PATH;
  static const std::string CONFIG_XSP_CONFIG_SAVE_PATH;
  static const std::string CONFIG_XSP_USE_RESGRADES;
  static const std::string CONFIG_XSP_RUN_FLAGS;
  static const std::string CONFIG_XSP_DTC_ENERGY;
  static const std::string CONFIG_XSP_TRIGGER_MODE;
  static const std::string CONFIG_XSP_INVERT_F0;
  static const std::string CONFIG_XSP_INVERT_VETO;
  static const std::string CONFIG_XSP_DEBOUNCE;
  static const std::string CONFIG_XSP_EXPOSURE_TIME;
  static const std::string CONFIG_XSP_FRAMES;
 
  /** Configuration constants for commands **/
  static const std::string CONFIG_CMD;
  static const std::string CONFIG_CMD_CONNECT;
  static const std::string CONFIG_CMD_SAVE;
  static const std::string CONFIG_CMD_RESTORE;
  static const std::string CONFIG_CMD_START;
  static const std::string CONFIG_CMD_STOP;
  static const std::string CONFIG_CMD_TRIGGER;

  void setupControlInterface(const std::string& ctrlEndpointString);
  void closeControlInterface();
  void runIpcService(void);
  void tickTimer(void);

  /** Pointer to the logging facility */
  log4cxx::LoggerPtr                                              logger_;
  /** Condition for exiting this file writing process */
  boost::condition_variable                                       exitCondition_;
  /** Mutex used for locking the exitCondition */
  boost::mutex                                                    exitMutex_;
  /** Used to check for Ipc tick timer termination */
  bool                                                            runThread_;
  /** Is the main thread running */
  bool                                                            threadRunning_;
  /** Did an error occur during the thread initialisation */
  bool                                                            threadInitError_;
  /** Have we successfully shutdown */
  bool                                                            shutdown_;
  /** Main thread used for control message handling */
  boost::thread                                                   ctrlThread_;
  /** Store for any messages occurring during thread initialisation */
  std::string                                                     threadInitMsg_;
  /** Pointer to the IpcReactor for msg handling */
  boost::shared_ptr<OdinData::IpcReactor>                         reactor_;
  /** End point for control messages */
  std::string                                                     ctrlChannelEndpoint_;
  /** ZMQ context for IPC channels */
  OdinData::IpcContext&                                           ipc_context_;
  /** IpcChannel for control messages */
  OdinData::IpcChannel                                            ctrlChannel_;
  /** The Xspress hardware wrapper object */
  XspressDetector                                                 xsp_;
};

} /* namespace Xspress */

#endif /* XspressController_H_ */
