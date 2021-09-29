/*
 * XspressController.cpp
 *
 *  Created on: 22 Sep 2021
 *      Author: Diamond Light Source
 */

#include <stdio.h>

#include "XspressController.h"
#include "DebugLevelLogger.h"
#include "version.h"

namespace Xspress
{

const std::string XspressController::CONFIG_SHUTDOWN            = "shutdown";

const std::string XspressController::CONFIG_DEBUG               = "debug_level";

const std::string XspressController::CONFIG_CTRL_ENDPOINT       = "ctrl_endpoint";


/** Construct a new XspressController class.
 *
 * The constructor sets up logging used within the class, and starts the
 * IpcReactor thread.
 */
XspressController::XspressController() :
    logger_(log4cxx::Logger::getLogger("Xspress.XspressController")),
    runThread_(true),
    threadRunning_(false),
    threadInitError_(false),
    shutdown_(false),
    ipc_context_(OdinData::IpcContext::Instance(1)),
    ctrlThread_(boost::bind(&XspressController::runIpcService, this)),
    ctrlChannelEndpoint_(""),
    ctrlChannel_(ZMQ_ROUTER)
{
  OdinData::configure_logging_mdc(OdinData::app_path.c_str());
  LOG4CXX_DEBUG_LEVEL(1, logger_, "Constructing XspressController");

  // Wait for the thread service to initialise and be running properly, so that
  // this constructor only returns once the object is fully initialised (RAII).
  // Monitor the thread error flag and throw an exception if initialisation fails
  while (!threadRunning_)
  {
    if (threadInitError_) {
      ctrlThread_.join();
      throw std::runtime_error(threadInitMsg_);
    }
  }
}

/**
 * Destructor.
 */
XspressController::~XspressController()
{
  // Make sure we shutdown cleanly if an exception was thrown
  shutdown();
}

/** Handle an incoming configuration message.
 *
 * This method is called by the IpcReactor when a configuration IpcMessage
 * has been received. The raw message is read and parsed into an IpcMessage
 * for further processing. The configure method is called, and once
 * configuration has completed a response IpcMessage is sent back on the
 * control channel.
 */
void XspressController::handleCtrlChannel()
{
  // Receive a message from the main thread channel
  std::string clientIdentity;
  std::string ctrlMsgEncoded = ctrlChannel_.recv(&clientIdentity);
  unsigned int msg_id = 0;
  
  LOG4CXX_DEBUG_LEVEL(3, logger_, "Control thread called with message: " << ctrlMsgEncoded);

  // Parse and handle the message
  try {
    OdinData::IpcMessage ctrlMsg(ctrlMsgEncoded.c_str());
    OdinData::IpcMessage replyMsg;  // Instantiate default IpmMessage
    replyMsg.set_msg_val(ctrlMsg.get_msg_val());
    msg_id = ctrlMsg.get_msg_id();
    replyMsg.set_msg_id(msg_id);

    if ((ctrlMsg.get_msg_type() == OdinData::IpcMessage::MsgTypeCmd) &&
        (ctrlMsg.get_msg_val()  == OdinData::IpcMessage::MsgValCmdConfigure)) {
      replyMsg.set_msg_type(OdinData::IpcMessage::MsgTypeAck);
      this->configure(ctrlMsg, replyMsg);
      LOG4CXX_DEBUG_LEVEL(3, logger_, "Control thread reply message (configure): "
                             << replyMsg.encode());
    }
    else if ((ctrlMsg.get_msg_type() == OdinData::IpcMessage::MsgTypeCmd) &&
             (ctrlMsg.get_msg_val() == OdinData::IpcMessage::MsgValCmdRequestConfiguration)) {
        replyMsg.set_msg_type(OdinData::IpcMessage::MsgTypeAck);
        this->requestConfiguration(replyMsg);
        LOG4CXX_DEBUG_LEVEL(3, logger_, "Control thread reply message (request configuration): "
                               << replyMsg.encode());
    }
    else if ((ctrlMsg.get_msg_type() == OdinData::IpcMessage::MsgTypeCmd) &&
             (ctrlMsg.get_msg_val() == OdinData::IpcMessage::MsgValCmdStatus)) {
      replyMsg.set_msg_type(OdinData::IpcMessage::MsgTypeAck);
      this->provideStatus(replyMsg);
      LOG4CXX_DEBUG_LEVEL(3, logger_, "Control thread reply message (status): "
                             << replyMsg.encode());
    }
    else if ((ctrlMsg.get_msg_type() == OdinData::IpcMessage::MsgTypeCmd) &&
             (ctrlMsg.get_msg_val() == OdinData::IpcMessage::MsgValCmdRequestVersion)) {
      replyMsg.set_msg_type(OdinData::IpcMessage::MsgTypeAck);
      this->provideVersion(replyMsg);
      LOG4CXX_DEBUG_LEVEL(3, logger_, "Control thread reply message (request version): "
                             << replyMsg.encode());
    }
    else if ((ctrlMsg.get_msg_type() == OdinData::IpcMessage::MsgTypeCmd) &&
             (ctrlMsg.get_msg_val() == OdinData::IpcMessage::MsgValCmdResetStatistics)) {
      replyMsg.set_msg_type(OdinData::IpcMessage::MsgTypeAck);
      this->resetStatistics(replyMsg);
      LOG4CXX_DEBUG_LEVEL(3, logger_, "Control thread reply message (reset statistics): "
              << replyMsg.encode());
    }
    else if ((ctrlMsg.get_msg_type() == OdinData::IpcMessage::MsgTypeCmd) &&
             (ctrlMsg.get_msg_val() == OdinData::IpcMessage::MsgValCmdShutdown)) {
      replyMsg.set_msg_type(OdinData::IpcMessage::MsgTypeAck);
      exitCondition_.notify_all();
      LOG4CXX_DEBUG_LEVEL(3, logger_, "Control thread reply message (shutdown): "
              << replyMsg.encode());
    }
    else {
      LOG4CXX_ERROR(logger_, "Control thread got unexpected message: " << ctrlMsgEncoded);
      replyMsg.set_param("error", "Invalid control message: " + ctrlMsgEncoded);
      replyMsg.set_msg_type(OdinData::IpcMessage::MsgTypeNack);
    }
    ctrlChannel_.send(replyMsg.encode(), 0, clientIdentity);
  }
  catch (OdinData::IpcMessageException& e)
  {
    LOG4CXX_ERROR(logger_, "Error decoding control channel request: " << e.what());
  }
  catch (std::runtime_error& e)
  {
    LOG4CXX_ERROR(logger_, "Bad control message: " << e.what());
    OdinData::IpcMessage replyMsg(OdinData::IpcMessage::MsgTypeNack, OdinData::IpcMessage::MsgValCmdConfigure);
    replyMsg.set_param<std::string>("error", std::string(e.what()));
    replyMsg.set_msg_id(msg_id);
    ctrlChannel_.send(replyMsg.encode());
  }
}

/** Provide status information to requesting clients.
 *
 * This is called in response to a status request from a connected client. The reply to the
 * request is populated with status information from the shared memory controller and all the
 * plugins currently loaded, and with any error messages currently stored.
 *
 * @param[in,out] reply - response IPC message to be populated with status parameters
 */
void XspressController::provideStatus(OdinData::IpcMessage& reply)
{
}

/** Provide version information to requesting clients.
 *
 * This is called in response to a version request from a connected client. The reply to the
 * request is populated with version information from application and all the
 * plugins currently loaded.
 *
 * @param[in,out] reply - response IPC message to be populated with version information
 */
void XspressController::provideVersion(OdinData::IpcMessage& reply)
{
  // Populate the reply with top-level odin-data application version information
  reply.set_param("version/xspress-detector/major", XSPRESS_DETECTOR_VERSION_MAJOR);
  reply.set_param("version/xspress-detector/minor", XSPRESS_DETECTOR_VERSION_MINOR);
  reply.set_param("version/xspress-detector/patch", XSPRESS_DETECTOR_VERSION_PATCH);
  reply.set_param("version/xspress-detector/short", std::string(XSPRESS_DETECTOR_VERSION_STR_SHORT));
  reply.set_param("version/xspress-detector/full", std::string(XSPRESS_DETECTOR_VERSION_STR)); 
}

/**
 * Set configuration options for the FrameProcessorController.
 *
 * Sets up the overall FileWriter application according to the
 * configuration IpcMessage objects that are received. The objects
 * are searched for:
 * CONFIG_SHUTDOWN - Shuts down the application
 * CONFIG_STATUS - Retrieves status for all plugins and replies
 * CONFIG_CTRL_ENDPOINT - Calls the method setupControlInterface
 * CONFIG_PLUGIN - Calls the method configurePlugin
 * CONFIG_FR_SETUP - Calls the method setupFrameReceiverInterface
 *
 * The method also searches for configuration objects that have the
 * same index as loaded plugins. If any of these are found the they
 * are passed down to the plugin for execution.
 *
 * \param[in] config - IpcMessage containing configuration data.
 * \param[out] reply - Response IpcMessage.
 */
void XspressController::configure(OdinData::IpcMessage& config, OdinData::IpcMessage& reply)
{
  LOG4CXX_DEBUG_LEVEL(1, logger_, "Configuration submitted: " << config.encode());

  // Check for a debug level change
  if (config.has_param(XspressController::CONFIG_DEBUG)) {
    unsigned int debug_level = config.get_param<unsigned int>(XspressController::CONFIG_DEBUG);
    LOG4CXX_DEBUG_LEVEL(1, logger_, "Debug level set to  " << debug_level);
    set_debug_level(debug_level);
  }

  if (config.has_param(XspressController::CONFIG_CTRL_ENDPOINT)) {
    std::string endpoint = config.get_param<std::string>(XspressController::CONFIG_CTRL_ENDPOINT);
    this->setupControlInterface(endpoint);
  }

}

/**
 * Request the current configuration of the FrameProcessorController.
 *
 * The method also searches through all loaded plugins.  Each plugin is
 * also sent a request for its configuration.
 *
 * \param[out] reply - Response IpcMessage with the current configuration.
 */
void XspressController::requestConfiguration(OdinData::IpcMessage& reply)
{
  LOG4CXX_DEBUG_LEVEL(3, logger_, "Request for configuration made");

  // Add local configuration parameter values to the reply
  reply.set_param(XspressController::CONFIG_CTRL_ENDPOINT, ctrlChannelEndpoint_);
}

/**
 * Reset statistics on all of the loaded plugins.
 *
 * The method calls reset statistics on all of the loaded plugins.
 *
 * \param[out] reply - Response IpcMessage with the current status.
 */
void XspressController::resetStatistics(OdinData::IpcMessage& reply)
{
  LOG4CXX_DEBUG_LEVEL(1, logger_, "Reset statistics requested");
  bool reset_ok = true;
}

void XspressController::run() {

  LOG4CXX_INFO(logger_, "Running frame processor");

  // Start worker thread (for IFrameCallback) to monitor frames passed through
//  start();

  // Now wait for the shutdown command from either the control interface or the worker thread
  waitForShutdown();
  shutdown();
}

void XspressController::shutdown() {
  if (!shutdown_) {
    LOG4CXX_INFO(logger_, "Received shutdown command");

    // Stop worker thread (for IFrameCallback) and reactor
    LOG4CXX_DEBUG_LEVEL(1, logger_, "Stopping FrameProcessorController worker thread and IPCReactor");
//    stop();
    reactor_->stop();

    // Close control IPC channel
    closeControlInterface();

    shutdown_ = true;
    LOG4CXX_INFO(logger_, "Shutting Down");
  }
}

/**
 * Wait for the exit condition before returning.
 */
void XspressController::waitForShutdown()
{
  boost::unique_lock<boost::mutex> lock(exitMutex_);
  exitCondition_.wait(lock);
}

/** Set up the control interface.
 *
 * This method binds the control IpcChannel to the provided endpoint,
 * creating a socket for controlling applications to connect to. This
 * socket is used for sending configuration IpcMessages.
 *
 * \param[in] ctrlEndpointString - Name of the control endpoint.
 */
void XspressController::setupControlInterface(const std::string& ctrlEndpointString)
{
  try {
    LOG4CXX_DEBUG_LEVEL(1, logger_, "Connecting control channel to endpoint: " << ctrlEndpointString);
    ctrlChannel_.bind(ctrlEndpointString.c_str());
    ctrlChannelEndpoint_ = ctrlEndpointString;
  }
  catch (zmq::error_t& e) {
    //std::stringstream ss;
    //ss << "RX channel connect to endpoint " << config_.rx_channel_endpoint_ << " failed: " << e.what();
    // TODO: What to do here, I think throw it up
    throw std::runtime_error(e.what());
  }

  // Add the control channel to the reactor
  reactor_->register_channel(ctrlChannel_, boost::bind(&XspressController::handleCtrlChannel, this));
}

/** Close the control interface.
 */
void XspressController::closeControlInterface()
{
  try {
    LOG4CXX_DEBUG_LEVEL(1, logger_, "Closing control endpoint socket.");
    ctrlThread_.join();
    reactor_->remove_channel(ctrlChannel_);
    ctrlChannel_.close();
  }
  catch (zmq::error_t& e) {
    // TODO: What to do here, I think throw it up
    throw std::runtime_error(e.what());
  }
}

/** Start the Ipc service running.
 *
 * Sets up a tick timer and runs the Ipc reactor.
 * Currently the tick timer does not perform any processing.
 */
void XspressController::runIpcService(void)
{
  // Configure logging for this thread
  OdinData::configure_logging_mdc(OdinData::app_path.c_str());

  LOG4CXX_DEBUG_LEVEL(1, logger_, "Running IPC thread service");

  // Create the reactor
  reactor_ = boost::shared_ptr<OdinData::IpcReactor>(new OdinData::IpcReactor());

  // Add the tick timer to the reactor
  int tick_timer_id = reactor_->register_timer(1000, 0, boost::bind(&XspressController::tickTimer, this));

  // Set thread state to running, allows constructor to return
  threadRunning_ = true;

  // Run the reactor event loop
  reactor_->run();

  // Cleanup - remove channels, sockets and timers from the reactor and close the receive socket
  LOG4CXX_DEBUG_LEVEL(1, logger_, "Terminating IPC thread service");
}

/** Tick timer task called by IpcReactor.
 *
 * This currently performs no processing.
 */
void XspressController::tickTimer(void)
{
  if (!runThread_)
  {
    LOG4CXX_DEBUG_LEVEL(1, logger_, "IPC thread terminate detected in timer");
    reactor_->stop();
  }
}

} /* namespace Xspress */
