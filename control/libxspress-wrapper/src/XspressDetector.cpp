/*
 * XspressDetector.cpp
 *
 *  Created on: 24 Sep 2021
 *      Author: Diamond Light Source
 */

#include <stdio.h>
#include "dirent.h"

#include "XspressDetector.h"
#include "DebugLevelLogger.h"

namespace Xspress
{
/** Construct a new XspressDetector class.
 *
 * The constructor sets up logging used within the class, and initialises
 * variables.
 */
XspressDetector::XspressDetector(bool simulation) :
    logger_(log4cxx::Logger::getLogger("Xspress.XspressDetector")),
    simulated_(simulation),
    connected_(false),
    reconnect_required_(false),
    xsp_num_cards_(0),
    xsp_num_tf_(0),
    xsp_base_IP_(""),
    xsp_max_channels_(0),
    xsp_max_spectra_(0),
    xsp_debug_(0),
    xsp_config_path_(""),
    xsp_config_save_path_(""),
    xsp_use_resgrades_(false),
    xsp_num_aux_data_(1),
    xsp_run_flags_(0),
    xsp_dtc_params_updated_(false),
    xsp_dtc_energy_(0.0),
    xsp_clock_period_(0),
    xsp_trigger_mode_(""),
    xsp_invert_f0_(0),
    xsp_invert_veto_(0),
    xsp_debounce_(0),
    xsp_exposure_time_(0.0),
    xsp_frames_(0),
    xsp_mode_(XSP_MODE_MCA)
{
  OdinData::configure_logging_mdc(OdinData::app_path.c_str());
  LOG4CXX_DEBUG_LEVEL(1, logger_, "Constructing XspressDetector");
}

/** Destructor for XspressDetector class.
 *
 */
XspressDetector::~XspressDetector()
{

}

void XspressDetector::setErrorString(const std::string& error)
{
  LOG4CXX_ERROR(logger_, error);
  error_string_ = error;
}

std::string XspressDetector::getErrorString()
{
  return error_string_;
}

std::string XspressDetector::getVersionString()
{
  std::string version = "Not connected";
  if (connected_){
    version = detector_.getVersionString();
  }
  return version;
}

int XspressDetector::connect()
{
  int status = XSP_STATUS_OK;
  if (!connected_){
    // Check the mode and then connect accordingly
    if (xsp_mode_ == XSP_MODE_MCA){
      status = connect_mca_mode();
    } else if (xsp_mode_ == XSP_MODE_LIST){
      status = connect_list_mode();
    } else {
      setErrorString("Invalid connection mode, could not connect: " + xsp_mode_);
      status = XSP_STATUS_ERROR;
    }
  } else {
    setErrorString("Xspress already connected, disconnect first");
    status = XSP_STATUS_ERROR;
  }
  return status;
}

int XspressDetector::connect_mca_mode()
{
  int status = XSP_STATUS_OK;
  // Reset connected status
  connected_ = false;

  // Check the IP address is not empty
  if (xsp_base_IP_ == ""){
    setErrorString("No connection IP address has been set");
    status = XSP_STATUS_ERROR;
  }

  if (status == XSP_STATUS_OK){
    if (simulated_){

    } else {
      // Call the detector configure method
      status = detector_.configure_mca(
        xsp_num_cards_,                            // Number of XSPRESS cards
        xsp_num_tf_,                               // Number of 4096 energy bin spectra timeframes
        const_cast<char *>(xsp_base_IP_.c_str()),  // Base IP address
        -1,                                        // Base port number override (-1 does not override)
        xsp_max_channels_,                         // Set the maximum number of channels
        xsp_debug_,                                // Enable debug messages
        0                                          // Enable verbose debug messages
      );
    }
    if (status == XSP_STATUS_OK){
      // We have a valid handle to set the connected status
      LOG4CXX_INFO(logger_, "Connected to Xspress");
      connected_ = true;
    }
  }
  return status;
}

int XspressDetector::connect_list_mode()
{
  int status = XSP_STATUS_OK;

  // Call the detector configure method
  status = detector_.configure_list(
    xsp_num_cards_,                            // Number of XSPRESS cards
    xsp_num_tf_,                               // Number of 4096 energy bin spectra timeframes
    const_cast<char *>(xsp_base_IP_.c_str()),  // Base IP address
    -1,                                        // Base port number override (-1 does not override)
    xsp_max_channels_,                         // Set the maximum number of channels
    xsp_debug_                                 // Enable debug messages
  );
  if (status == XSP_STATUS_OK){
    // We have a valid handle to set the connected status
    LOG4CXX_INFO(logger_, "Connected to Xspress");
    connected_ = true;
  }
  return status;
}

int XspressDetector::checkConnected()
{
  return connected_;
}

int XspressDetector::disconnect()
{
  int status = XSP_STATUS_OK;

  if (checkConnected()){
    status = detector_.close_connection();
    if (status == XSP_STATUS_OK){
      // We have disconnected from the detector
      LOG4CXX_INFO(logger_, "Disconnected from Xspress");
      connected_ = false;
    }

    // Shutdown the DAQ object and threads if required
    if (daq_){
      daq_.reset();
    }
  }
  return status;
}

int XspressDetector::setupChannels()
{
  int status = XSP_STATUS_OK;
  if (checkConnected()){
    status = detector_.check_connected_channels(cards_connected_, channels_connected_);
  } else {
    LOG4CXX_INFO(logger_, "Cannot set up channels as not connected");
  }
  return status;
}

int XspressDetector::enableDAQ()
{
  int status = XSP_STATUS_OK;
  if (checkConnected()){
    if (xsp_daq_endpoints_.size() > 0){
      LOG4CXX_INFO(logger_, "XspressDetector creating DAQ object");
      // Create the DAQ object
      daq_ = boost::shared_ptr<XspressDAQ>(new XspressDAQ(&detector_, xsp_max_channels_, xsp_max_spectra_, xsp_daq_endpoints_));
      // Setup DAQ object with num_aux_data
      daq_->set_num_aux_data(xsp_num_aux_data_);
    } else {
      LOG4CXX_ERROR(logger_, "Cannot set up DAQ as no endpoints have been specified");
      status = XSP_STATUS_ERROR;
    }
  } else {
    LOG4CXX_ERROR(logger_, "Cannot set up DAQ as not connected");
    status = XSP_STATUS_ERROR;
  }
  return status;
}

/**
 * Check if a save directory is empty. Return an error if not.
 * This is to prevent users overwriting existing config files.
 */
int XspressDetector::checkSaveDir(const std::string& dir_name)
{
  int status = XSP_STATUS_OK;
  int countFiles = 0;
  struct dirent *d = NULL;

  DIR *dir = opendir(dir_name.c_str());
  if (dir == NULL){
    //Directory doesn't exist.
    setErrorString("Cannot open save directory.");
    status = XSP_STATUS_ERROR;
  }
  if (status != XSP_STATUS_ERROR){
    while ((d = readdir(dir)) != NULL){
      if (++countFiles > 2){
        setErrorString("Files already exist in the save directory.");
        status = XSP_STATUS_ERROR;
        break;
      }
    }
  }
  closedir(dir);

  return status;
}

/**
 * Save the system settings for the xspress3 system. 
 * This simply calls xsp3_save_settings().
 */
int XspressDetector::saveSettings()
{
  int status = XSP_STATUS_OK;
  LOG4CXX_INFO(logger_, "Saving Xspress settings.");

  if (!connected_){
    setErrorString("Cannot save settings, not connected");
    status = XSP_STATUS_ERROR;
  } else if (xsp_config_save_path_ == ""){
    setErrorString("Cannot save settings, no config save path set");
    status = XSP_STATUS_ERROR;
  } else {
    status = detector_.save_settings(xsp_config_save_path_);
    if (status == XSP_STATUS_OK) {
      LOG4CXX_INFO(logger_, "Saved Configuration.");
    }
  }
  return status;
}

int XspressDetector::restoreSettings()
{
  int status = XSP_STATUS_OK;
  int xsp_status = 0;
  if (!connected_){
    setErrorString("Cannot restore settings, not connected");
    status = XSP_STATUS_ERROR;
  } else if (xsp_config_path_ == ""){
    setErrorString("Cannot restore settings, no config path set");
    status = XSP_STATUS_ERROR;
  } else {
    status = detector_.restore_settings(xsp_config_path_);
    if (status == XSP_STATUS_OK){
      LOG4CXX_INFO(logger_, "Restored Xspress configuration");
    }
  }

  // Set up resgrades
  if (status == XSP_STATUS_OK){
    status = detector_.setup_resgrades(xsp_use_resgrades_, xsp_max_channels_, xsp_num_aux_data_);
    if (status == XSP_STATUS_OK){
      LOG4CXX_DEBUG_LEVEL(1, logger_, "xsp_num_aux_data set to " << xsp_num_aux_data_);
      // If the DAQ object exists then setup the aux_data value
      if (daq_){
        // Setup DAQ object with num_aux_data
        daq_->set_num_aux_data(xsp_num_aux_data_);
      }
    } else {
      setErrorString(detector_.getErrorString());
    }
  }

  // Apply run flags parameter
  if (status == XSP_STATUS_OK){
    status = detector_.set_run_flags(xsp_run_flags_);
    if (status != XSP_STATUS_OK){
      setErrorString(detector_.getErrorString());
    }
  }

  // Read existing SCA params
  if (status == XSP_STATUS_OK){
    status = readSCAParams();
    if (status != XSP_STATUS_OK){
      setErrorString(detector_.getErrorString());
    }
  }

  // Read the DTC parameters
  if (status == XSP_STATUS_OK){
    status = readDTCParams();
    if (status != XSP_STATUS_OK){
      setErrorString(detector_.getErrorString());
    }
  }

  // We ensure here that DTC energy is set between application restart and frame acquisition
  if (status == XSP_STATUS_OK){
    status = detector_.set_dtc_energy(xsp_dtc_energy_);
    if (status != XSP_STATUS_OK){
      setErrorString(detector_.getErrorString());
    }
  }

  // Read the clock period
  if (status == XSP_STATUS_OK){
    status = detector_.get_clock_period(xsp_clock_period_);
    if (status != XSP_STATUS_OK){
      setErrorString(detector_.getErrorString());
    }
  }

  // Set the reconnect flag if something has gone wrong with this configuration
  reconnect_required_ = (status != XSP_STATUS_OK);

  // Re-apply trigger mode setting, since may have been overridden by restored config
  if (status == XSP_STATUS_OK){
    status = setTriggerMode();
    if (status != XSP_STATUS_OK){
      setErrorString(detector_.getErrorString());
    }
  }

  return status;
}

/**
 * Read the SCA window limits (for SCA 5 and 6) and threshold for SCA 4, for each channel.
 */
int XspressDetector::readSCAParams()
{

  return detector_.read_sca_params(xsp_max_channels_,
                                   xsp_chan_sca5_low_lim_,
                                   xsp_chan_sca5_high_lim_,
                                   xsp_chan_sca6_low_lim_,
                                   xsp_chan_sca6_high_lim_,
                                   xsp_chan_sca4_threshold_
                                   );
}

void XspressDetector::readFemStatus()
{
  int status = XSP_STATUS_OK;

  // number of frames read out for each channel
  status = detector_.read_frames(xsp_max_channels_, xsp_status_frames_);
  if (status != XSP_STATUS_OK){
    setErrorString("Cannot read frame counters");
  }
  // read dropped frames
  status = detector_.read_dropped_frames(xsp_status_dropped_frames_);
  if (status != XSP_STATUS_OK){
    setErrorString("Cannot read dropped frame counters");
  }

  // read temperatures
  status = detector_.read_temperatures(xsp_status_temperature_0_,
                                       xsp_status_temperature_1_,
                                       xsp_status_temperature_2_,
                                       xsp_status_temperature_3_,
                                       xsp_status_temperature_4_,
                                       xsp_status_temperature_5_);
  if (status != XSP_STATUS_OK){
    setErrorString("Cannot read temperatures");
  }
}

/**
 * Read the dead time correction (DTC) parameters for each channel.
 */
int XspressDetector::readDTCParams()
{
  return detector_.read_dtc_params(xsp_max_channels_, pDTCi_, pDTCd_, xsp_dtc_params_updated_);
}

/**
 * Write new dead time correction (DTC) parameters for each channel.
 */
int XspressDetector::writeDTCParams()
{
  return detector_.write_dtc_params(xsp_max_channels_, pDTCi_, pDTCd_);
}

int XspressDetector::setTriggerMode()
{
  return detector_.setTriggerMode(xsp_frames_,
                                  xsp_exposure_time_,
                                  xsp_clock_period_,
                                  xsp_trigger_mode_,
                                  xsp_debounce_,
                                  xsp_invert_f0_,
                                  xsp_invert_veto_);
}

int XspressDetector::startAcquisition()
{
  int status = XSP_STATUS_OK;
  // Check we are connected to the hardware
  LOG4CXX_INFO(logger_, "Arming detector for data collection");

  if (checkConnected()){
    // Set the trigger mode
    status = setTriggerMode();
  } else {
    setErrorString("Cannot start acquisition as we are not connected");
    status = XSP_STATUS_ERROR;
  }

  if (status == XSP_STATUS_OK){
    // Arm the child cards to recieve TTL Veto signals.
    for (int card_n = 1; card_n < xsp_num_cards_; card_n++) {
      status |= detector_.histogram_start(card_n);
    }
  }

  if (status == XSP_STATUS_OK){
    if (xsp_trigger_mode_ == TM_SOFTWARE_STR){
      // Arm for soft trigger
      status = detector_.histogram_arm(0);
    } else {
      // Start the detector
      status = detector_.histogram_start(0);
    }
  }

  if (xsp_mode_ == XSP_MODE_MCA){
    if (status == XSP_STATUS_OK){
      // If the DAQ object exists prime the DAQ threads with the expected number of frames
      if (daq_){
        daq_->startAcquisition(xsp_frames_);
      }
    }
  } else {
    LOG4CXX_INFO(logger_, "Arming for list mode, disabling control DAQ");
  }

  if (status == XSP_STATUS_OK){
    LOG4CXX_INFO(logger_, "Arm complete, detector ready for acquisition");
    acquiring_ = true;
  }
  return status;
}

int XspressDetector::stopAcquisition()
{
  int status = XSP_STATUS_OK;
  if (acquiring_){
    status = detector_.histogram_stop(-1);
  }
  return status;
}

int XspressDetector::sendSoftwareTrigger()
{
  int status = XSP_STATUS_OK;
  if (acquiring_){
    if (xsp_trigger_mode_ == TM_SOFTWARE_STR){
      status = detector_.histogram_continue(0);
      status |= detector_.histogram_pause(0);
    } else {
      setErrorString("Cannot send software trigger, trigger_mode is not [software]");
      status = XSP_STATUS_ERROR;
    }
  } else {
    setErrorString("Cannot send software trigger if not acquiring");
    status = XSP_STATUS_ERROR;
  }
  return status;
}

void XspressDetector::setXspNumCards(int num_cards)
{
  xsp_num_cards_ = num_cards;

  // Re initialise the connection vectors based on the number of cards
  cards_connected_.clear();
  cards_connected_.resize(xsp_num_cards_);
  channels_connected_.clear();
  channels_connected_.resize(xsp_num_cards_);

  // Re initialise the dropped frame vectors based on the number of cards
  xsp_status_dropped_frames_.clear();
  xsp_status_dropped_frames_.resize(xsp_num_cards_);

  // Re initialise the temperature status arrays based on the number of cards
  xsp_status_temperature_0_.clear();
  xsp_status_temperature_0_.resize(xsp_num_cards_);
  xsp_status_temperature_1_.clear();
  xsp_status_temperature_1_.resize(xsp_num_cards_);
  xsp_status_temperature_2_.clear();
  xsp_status_temperature_2_.resize(xsp_num_cards_);
  xsp_status_temperature_3_.clear();
  xsp_status_temperature_3_.resize(xsp_num_cards_);
  xsp_status_temperature_4_.clear();
  xsp_status_temperature_4_.resize(xsp_num_cards_);
  xsp_status_temperature_5_.clear();
  xsp_status_temperature_5_.resize(xsp_num_cards_);

}

int XspressDetector::getXspNumCards()
{
  return xsp_num_cards_;
}

void XspressDetector::setXspNumTf(int num_tf)
{
  xsp_num_tf_ = num_tf;
}

int XspressDetector::getXspNumTf()
{
  return xsp_num_tf_;
}

void XspressDetector::setXspBaseIP(const std::string& address)
{
  xsp_base_IP_ = address;
}

std::string XspressDetector::getXspBaseIP()
{
  return xsp_base_IP_;
}

void XspressDetector::setXspMaxChannels(int max_channels)
{
  xsp_max_channels_ = max_channels;

  // Re initialise the frames read vector based on the maximum channels
  xsp_status_frames_.clear();
  xsp_status_frames_.resize(xsp_max_channels_);

  // Re initialise the DTC vectors based on the maximum channels
  pDTCi_.clear();
  pDTCi_.resize(XSP3_NUM_DTC_INT_PARAMS * xsp_max_channels_);
  pDTCd_.clear();
  pDTCd_.resize(XSP3_NUM_DTC_FLOAT_PARAMS * xsp_max_channels_);
}

int XspressDetector::getXspMaxChannels()
{
  return xsp_max_channels_;
}

void XspressDetector::setXspMaxSpectra(int max_spectra)
{
  xsp_max_spectra_ = max_spectra;
}

int XspressDetector::getXspMaxSpectra()
{
  return xsp_max_spectra_;
}

void XspressDetector::setXspDebug(int debug)
{
  xsp_debug_ = debug;
}

int XspressDetector::getXspDebug()
{
  return xsp_debug_;
}

void XspressDetector::setXspConfigPath(const std::string& config_path)
{
  xsp_config_path_ = config_path;
}

std::string XspressDetector::getXspConfigPath()
{
  return xsp_config_path_;
}

void XspressDetector::setXspConfigSavePath(const std::string& config_save_path)
{
  xsp_config_save_path_ = config_save_path;
}

std::string XspressDetector::getXspConfigSavePath()
{
  return xsp_config_save_path_;
}

void XspressDetector::setXspUseResgrades(bool use)
{
  xsp_use_resgrades_ = use;
}

bool XspressDetector::getXspUseResgrades()
{
  return xsp_use_resgrades_;
}

void XspressDetector::setXspRunFlags(int flags)
{
  xsp_run_flags_ = flags;
}

int XspressDetector::getXspRunFlags()
{
  return xsp_run_flags_;
}

void XspressDetector::setXspDTCEnergy(double energy)
{
  xsp_dtc_energy_ = energy;
}

double XspressDetector::getXspDTCEnergy()
{
  return xsp_dtc_energy_;
}

void XspressDetector::setXspTriggerMode(const std::string& mode)
{
  xsp_trigger_mode_ = mode;
}

std::string XspressDetector::getXspTriggerMode()
{
  return xsp_trigger_mode_;
}

void XspressDetector::setXspInvertF0(int invert_f0)
{
  xsp_invert_f0_ = invert_f0;
}

int XspressDetector::getXspInvertF0()
{
  return xsp_invert_f0_;
}

void XspressDetector::setXspInvertVeto(int invert_veto)
{
  xsp_invert_veto_ = invert_veto;
}

int XspressDetector::getXspInvertVeto()
{
  return xsp_invert_veto_;
}

void XspressDetector::setXspDebounce(int debounce)
{
  xsp_debounce_ = debounce;
}

int XspressDetector::getXspDebounce()
{
  return xsp_debounce_;
}

void XspressDetector::setXspExposureTime(double exposure)
{
  xsp_exposure_time_ = exposure;
}

double XspressDetector::getXspExposureTime()
{
  return xsp_exposure_time_;
}

void XspressDetector::setXspFrames(int frames)
{
  xsp_frames_ = frames;
}

int XspressDetector::getXspFrames()
{
  return xsp_frames_;
}

void XspressDetector::setXspMode(const std::string& mode)
{
  xsp_mode_ = mode;
}

std::string XspressDetector::getXspMode()
{
  return xsp_mode_;
}

void XspressDetector::setXspDAQEndpoints(std::vector<std::string> endpoints)
{
  xsp_daq_endpoints_ = endpoints;
}

std::vector<std::string> XspressDetector::getXspDAQEndpoints()
{
  return xsp_daq_endpoints_;
}

} /* namespace Xspress */
