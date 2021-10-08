/*
 * LibXspressWrapper.cpp
 *
 *  Created on: 24 Sep 2021
 *      Author: Diamond Light Source
 */

#include <stdio.h>

#include "LibXspressWrapper.h"
#include "DebugLevelLogger.h"

namespace Xspress
{
const int N_RESGRADES = 16;

//Definitions of static class data members
const int LibXspressWrapper::runFlag_MCA_SPECTRA_ = 0;
const int LibXspressWrapper::runFlag_SCALERS_ONLY_ = 1;
const int LibXspressWrapper::runFlag_PLAYB_MCA_SPECTRA_ = 2;

/** Construct a new LibXspressWrapper class.
 *
 * The constructor sets up logging used within the class, and initialises
 * variables.
 */
LibXspressWrapper::LibXspressWrapper(bool simulation) :
    logger_(log4cxx::Logger::getLogger("Xspress.LibXspressWrapper")),
    simulated_(simulation),
    connected_(false),
    reconnect_required_(false),
    xsp_handle_(-1),
    xsp_num_cards_(0),
    xsp_num_tf_(0),
    xsp_base_IP_(""),
    xsp_max_channels_(0),
    xsp_debug_(0),
    xsp_config_path_(""),
    xsp_config_save_path_(""),
    xsp_use_resgrades_(false),
    xsp_num_aux_data_(0),
    xsp_run_flags_(0),
    xsp_dtc_params_updated_(false),
    xsp_dtc_energy_(0.0),
    xsp_clock_period_(0),
    xsp_trigger_mode_(0),
    xsp_invert_f0_(0),
    xsp_invert_veto_(0),
    xsp_debounce_(0),
    xsp_exposure_time_(0.0),
    xsp_frames_(0)
{
  OdinData::configure_logging_mdc(OdinData::app_path.c_str());
  LOG4CXX_DEBUG_LEVEL(1, logger_, "Constructing LibXspressWrapper");
}

/** Destructor for LibXspressWrapper class.
 *
 */
LibXspressWrapper::~LibXspressWrapper()
{

}

void LibXspressWrapper::setErrorString(const std::string& error)
{
  LOG4CXX_DEBUG_LEVEL(1, logger_, error);
  error_string_ = error;
}

std::string LibXspressWrapper::getErrorString()
{
  return error_string_;
}

void LibXspressWrapper::checkErrorCode(const std::string& prefix, int code)
{
  std::stringstream err;
  switch (code){
    case XSP3_OK:
      // No error here, pass through
      break;
    case XSP3_ERROR:
      err << prefix << " error [" << code << "] XSP3_ERROR";
      break;
    case XSP3_INVALID_PATH:
      err << prefix << " error [" << code << "] XSP3_INVALID_PATH";
      break;
    case XSP3_ILLEGAL_CARD:
      err << prefix << " error [" << code << "] XSP3_ILLEGAL_CARD";
      break;
    case XSP3_ILLEGAL_SUBPATH:
      err << prefix << " error [" << code << "] XSP3_ILLEGAL_SUBPATH";
      break;
    case XSP3_INVALID_DMA_STREAM:
      err << prefix << " error [" << code << "] XSP3_INVALID_DMA_STREAM";
      break;
    case XSP3_RANGE_CHECK:
      err << prefix << " error [" << code << "] XSP3_RANGE_CHECK";
      break;
    case XSP3_INVALID_SCOPE_MOD:
      err << prefix << " error [" << code << "] XSP3_INVALID_SCOPE_MOD";
      break;
    case XSP3_OUT_OF_MEMORY:
      err << prefix << " error [" << code << "] XSP3_OUT_OF_MEMORY";
      break;
    case XSP3_ERR_DEV_NOT_FOUND:
      err << prefix << " error [" << code << "] XSP3_ERR_DEV_NOT_FOUND";
      break;
    case XSP3_CANNOT_OPEN_FILE:
      err << prefix << " error [" << code << "] XSP3_CANNOT_OPEN_FILE";
      break;
    case XSP3_FILE_READ_FAILED:
      err << prefix << " error [" << code << "] XSP3_FILE_READ_FAILED";
      break;
    case XSP3_FILE_WRITE_FAILED:
      err << prefix << " error [" << code << "] XSP3_FILE_WRITE_FAILED";
      break;
    case XSP3_FILE_RENAME_FAILED:
      err << prefix << " error [" << code << "] XSP3_FILE_RENAME_FAILED";
      break;
    case XSP3_LOG_FILE_MISSING:
      err << prefix << " error [" << code << "] XSP3_LOG_FILE_MISSING";
      break;
    default:
      // Uknown error code here
      err << prefix << " error [" << code << "] Unknown error code";
  }
  if (code != XSP3_OK){
    setErrorString(err.str());
  }
}

int LibXspressWrapper::connect()
{
  int status = XSP_STATUS_OK;
  // Reset connected status
  connected_ = false;

  if (simulated_){

  } else {
    xsp_handle_ = xsp3_config(
      xsp_num_cards_,                             // Number of XSPRESS cards
      xsp_num_tf_,                                // Number of 4096 energy bin spectra timeframes
      const_cast<char *>(xsp_base_IP_.c_str()),   // Base IP address
      -1,                                         // Base port number override (-1 does not override)
      NULL,                                       // Base MAC override (NULL does not override)
      xsp_max_channels_,                          // Set the maximum number of channels
      1,                                          // Don't create scope data module
      NULL,                                       // Override scope data module filename
      xsp_debug_,                                 // Enable debug messages
      0                                           // Enable verbose debug messages
    );
  }
  // Check the returned handle.  
  // If the handle is less than zero then set an error
  if (xsp_handle_ < 0){
    status = XSP_STATUS_ERROR;
    checkErrorCode("xsp3_config", xsp_handle_);
  } else {
    // We have a valid handle to set the connected status
    LOG4CXX_INFO(logger_, "Connected to Xspress");
    connected_ = true;
  }
  return status;
}

int LibXspressWrapper::checkConnected()
{

}

/**
 * Save the system settings for the xspress3 system. 
 * This simply calls xsp3_save_settings().
 */
int LibXspressWrapper::saveSettings()
{
  int status = XSP_STATUS_OK;
  int xsp_status = 0;

  LOG4CXX_INFO(logger_, "Saving Xspress3 settings. This calls xsp3_save_settings().");

  if (!connected_){
    setErrorString("Cannot save settings, not connected");
    status = XSP_STATUS_ERROR;
  } else if (xsp_config_save_path_ == ""){
    setErrorString("Cannot save settings, no config save path set");
    status = XSP_STATUS_ERROR;
  } else {
    xsp_status = xsp3_save_settings(xsp_handle_, const_cast<char *>(xsp_config_save_path_.c_str()));
    if (xsp_status != XSP3_OK) {
      checkErrorCode("xsp3_save_settings", xsp_status);
      status = XSP_STATUS_ERROR;
    } else {
      LOG4CXX_INFO(logger_, "Saved Configuration.");
    }
  }
  return status;
}

int LibXspressWrapper::restoreSettings()
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
    xsp_status = xsp3_restore_settings_and_clock(
      xsp_handle_, 
      const_cast<char *>(xsp_config_path_.c_str()),
      0
    );
    if (xsp_status != XSP3_OK) {
      checkErrorCode("xsp3_restore_settings_and_clock", xsp_status);
      status = XSP_STATUS_ERROR;
    } else {
      LOG4CXX_INFO(logger_, "Restored Xspress configuration");
    }

    for (int chan = 0; chan < xsp_max_channels_; chan++) {
      int aux_mode = 0;
      if (xsp_use_resgrades_){
        xsp_num_aux_data_ = N_RESGRADES;
        aux_mode = XSP3_FORMAT_RES_MODE_MINDIV8;
      } else {
        xsp_num_aux_data_ = 1;
      }
      xsp_status = xsp3_format_run(xsp_handle_, chan, aux_mode, 0, 0, 0, 0, 12);
      if (xsp_status < XSP3_OK) {
        checkErrorCode("xsp3_format_run", xsp_status);
        status = XSP_STATUS_ERROR;
      } else {
        LOG4CXX_INFO(logger_, "Channel: " << chan << ", Number of time frames configured: " << xsp_status);
      }
    }

    // Apply run flags parameter
    switch (xsp_run_flags_)
    {
      case runFlag_SCALERS_ONLY_:
        // TODO: setting scaler only run flags doesn't seem to work (docs say it is possible but "not fully supported").
        //       doing so seems to result in no data being returned, or at least reported as available by
        //       xsp3_scaler_check_progress_details. Treat as per "runFlag_MCA_SPECTRA_" here for now.
        // xsp3_status = xsp3_set_run_flags(xsp3_handle_, XSP3_RUN_FLAGS_SCALERS | XSP3_RUN_FLAGS_CIRCULAR_BUFFER);
        // break;
      case runFlag_MCA_SPECTRA_:
        xsp_status = xsp3_set_run_flags(xsp_handle_, XSP3_RUN_FLAGS_SCALERS |
                                                     XSP3_RUN_FLAGS_HIST | 
                                                     XSP3_RUN_FLAGS_CIRCULAR_BUFFER);
        break;
      case runFlag_PLAYB_MCA_SPECTRA_:
        xsp_status = xsp3_set_run_flags(xsp_handle_, XSP3_RUN_FLAGS_PLAYBACK |
                                                     XSP3_RUN_FLAGS_SCALERS | 
                                                     XSP3_RUN_FLAGS_HIST |
                                                     XSP3_RUN_FLAGS_CIRCULAR_BUFFER);
        break;
      default:
        LOG4CXX_ERROR(logger_, "Invalid run flag option when trying to set xsp3_set_run_flags.");
        status = XSP_STATUS_ERROR;
        break;
    }

    if (xsp_status < XSP3_OK) {
      checkErrorCode("xsp3_set_run_flags", xsp_status);
      status = XSP_STATUS_ERROR;
    }

    // Read existing SCA params
    if (status == XSP_STATUS_OK){
      status = readSCAParams();
    }

    // Read the DTC parameters
    if (status == XSP_STATUS_OK){
      status = readDTCParams();
    }

    // We ensure here that DTC energy is set between application restart and frame
    // acquisition; otherwise libxspress3 would use an unspecified DTC energy.
    xsp_status = xsp3_setDeadtimeCalculationEnergy(xsp_handle_, xsp_dtc_energy_);
    if (xsp_status != XSP3_OK) {
        checkErrorCode("xsp3_setDeadtimeCalculationEnergy", xsp_status);
        status = XSP_STATUS_ERROR;
    }

    // Read the clock period
    xsp_clock_period_ = xsp3_get_clock_period(xsp_handle_, 0);

    // Set the reconnect flag if something has gone wrong with this configuration
    reconnect_required_ = (status != XSP_STATUS_OK);

    // Re-apply trigger mode setting, since may have been overridden by restored config
    status = setTriggerMode();

    return status;
  }
}

/**
 * Read the SCA window limits (for SCA 5 and 6) and threshold for SCA 4, for each channel.
 */
int LibXspressWrapper::readSCAParams()
{
  int status = XSP_STATUS_OK;
  int xsp_status = 0;
  uint32_t xsp_sca_param1 = 0;
  uint32_t xsp_sca_param2 = 0;

  // Clear the arrays
  xsp_chan_sca5_low_lim_.clear();
  xsp_chan_sca5_high_lim_.clear();
  xsp_chan_sca6_low_lim_.clear();
  xsp_chan_sca6_high_lim_.clear();
  xsp_chan_sca4_threshold_.clear();

  for (int chan = 0; chan < xsp_max_channels_; ++chan) {
    //SCA 5 window limits
    xsp_status = xsp3_get_window(xsp_handle_, chan, 0, &xsp_sca_param1, &xsp_sca_param2);
    if (xsp_status < XSP3_OK) {
      checkErrorCode("xsp3_get_window", xsp_status);
      status = XSP_STATUS_ERROR;
    } else {
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Read back SCA5 window limits: " << xsp_sca_param1 << ", " << xsp_sca_param2);
      xsp_chan_sca5_low_lim_.push_back(xsp_sca_param1);
      xsp_chan_sca5_high_lim_.push_back(xsp_sca_param2);
    }
    //SCA 6 window limits
    xsp_status = xsp3_get_window(xsp_handle_, chan, 1, &xsp_sca_param1, &xsp_sca_param2);
    if (xsp_status < XSP3_OK) {
      checkErrorCode("xsp3_get_window", xsp_status);
      status = XSP_STATUS_ERROR;
    } else {
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Read back SCA6 window limits: " << xsp_sca_param1 << ", " << xsp_sca_param2);
      xsp_chan_sca6_low_lim_.push_back(xsp_sca_param1);
      xsp_chan_sca6_high_lim_.push_back(xsp_sca_param2);
    }
    //SCA 4 threshold limit
    xsp_status = xsp3_get_good_thres(xsp_handle_, chan, &xsp_sca_param1);
    if (xsp_status < XSP3_OK) {
      checkErrorCode("xsp3_get_good thres", xsp_status);
      status = XSP_STATUS_ERROR;
    } else {
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Read back SCA4 threshold limit: " << xsp_sca_param1);
      xsp_chan_sca4_threshold_.push_back(xsp_sca_param1);
    }
  }
  return status;
}

/**
 * Read the dead time correction (DTC) parameters for each channel.
 */
int LibXspressWrapper::readDTCParams()
{
  int status = XSP_STATUS_OK;
  int xsp_status = 0;
  int xsp_dtc_flags = 0;
  double xsp_dtc_all_event_off = 0.0;
  double xsp_dtc_all_event_grad = 0.0;
  double xsp_dtc_all_event_rate_off = 0.0;
  double xsp_dtc_all_event_rate_grad = 0.0;
  double xsp_dtc_in_window_off = 0.0;
  double xsp_dtc_in_window_grad = 0.0;
  double xsp_dtc_in_window_rate_off = 0.0;
  double xsp_dtc_in_window_rate_grad = 0.0;

  for (int chan = 0; chan < xsp_max_channels_; chan++) {
    xsp_status = xsp3_getDeadtimeCorrectionParameters2(xsp_handle_,
                                                       chan,
                                                       &xsp_dtc_flags,
                                                       &xsp_dtc_all_event_off,
                                                       &xsp_dtc_all_event_grad,
                                                       &xsp_dtc_all_event_rate_off,
                                                       &xsp_dtc_all_event_rate_grad,
                                                       &xsp_dtc_in_window_off,
                                                       &xsp_dtc_in_window_grad,
                                                       &xsp_dtc_in_window_rate_off,
                                                       &xsp_dtc_in_window_rate_grad);
    if (xsp_status < XSP3_OK){
      checkErrorCode("xsp3_getDeadtimeCorrectionParameters", xsp_status);
      status = XSP_STATUS_ERROR;
    } else {
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Channel " << chan <<
                                      " Dead Time Correction Params: Flags: " << xsp_dtc_flags <<
                                      ", All Event Grad: " << xsp_dtc_all_event_grad <<
                                      ", All Event Off: " << xsp_dtc_all_event_off <<
                                      ", In Win Off: " << xsp_dtc_in_window_off <<
                                      ", In Win Grad: " << xsp_dtc_in_window_grad);

      pDTCi_[chan * XSP3_NUM_DTC_INT_PARAMS + XSP3_DTC_FLAGS] = xsp_dtc_flags;

      xsp_dtc_params_updated_ = xsp_dtc_params_updated_ ||
                         pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_AEO] != xsp_dtc_all_event_off;
      pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_AEO] = xsp_dtc_all_event_off;

      xsp_dtc_params_updated_ = xsp_dtc_params_updated_ ||
                         pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_AEG] != xsp_dtc_all_event_grad;
      pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_AEG] = xsp_dtc_all_event_grad;

      xsp_dtc_params_updated_ = xsp_dtc_params_updated_ ||
                         pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_AERO] != xsp_dtc_all_event_rate_off;
      pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_AERO] = xsp_dtc_all_event_rate_off;

      xsp_dtc_params_updated_ = xsp_dtc_params_updated_ ||
                         pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_AERG] != xsp_dtc_all_event_rate_grad;
      pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_AERG] = xsp_dtc_all_event_rate_grad;

      xsp_dtc_params_updated_ = xsp_dtc_params_updated_ ||
                         pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_IWO] != xsp_dtc_in_window_off;
      pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_IWO] = xsp_dtc_in_window_off;

      xsp_dtc_params_updated_ = xsp_dtc_params_updated_ ||
                         pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_IWG] != xsp_dtc_in_window_grad;
      pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_IWG] = xsp_dtc_in_window_grad;

      xsp_dtc_params_updated_ = xsp_dtc_params_updated_ ||
                         pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_IWRO] != xsp_dtc_in_window_rate_off;
      pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_IWRO] = xsp_dtc_in_window_rate_off;

      xsp_dtc_params_updated_ = xsp_dtc_params_updated_ ||
                         pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_IWRG] != xsp_dtc_in_window_rate_grad;
      pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_IWRG] = xsp_dtc_in_window_rate_grad;

    }
  }
  return status;
}

/**
 * Write new dead time correction (DTC) parameters for each channel.
 */
int LibXspressWrapper::writeDTCParams()
{
  int status = XSP_STATUS_OK;
  int xsp_status = 0;
  int xsp_dtc_flags = 0;
  double xsp_dtc_all_event_off = 0.0;
  double xsp_dtc_all_event_grad = 0.0;
  double xsp_dtc_all_event_rate_off = 0.0;
  double xsp_dtc_all_event_rate_grad = 0.0;
  double xsp_dtc_in_window_off = 0.0;
  double xsp_dtc_in_window_grad = 0.0;
  double xsp_dtc_in_window_rate_off = 0.0;
  double xsp_dtc_in_window_rate_grad = 0.0;

  for (int chan = 0; chan < xsp_max_channels_; chan++) {

    xsp_dtc_flags = pDTCi_[chan * XSP3_NUM_DTC_INT_PARAMS + XSP3_DTC_FLAGS];
    xsp_dtc_all_event_off = pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_AEO];
    xsp_dtc_all_event_grad = pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_AEG];
    xsp_dtc_all_event_rate_off = pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_AERO];
    xsp_dtc_all_event_rate_grad = pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_AERG];
    xsp_dtc_in_window_off = pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_IWO];
    xsp_dtc_in_window_grad = pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_IWG];
    xsp_dtc_in_window_rate_off = pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_IWRO];
    xsp_dtc_in_window_rate_grad = pDTCd_[chan * XSP3_NUM_DTC_FLOAT_PARAMS + XSP3_DTC_IWRG];

    xsp_status = xsp3_setDeadtimeCorrectionParameters2(xsp_handle_,
                                                       chan,
                                                       xsp_dtc_flags,
                                                       xsp_dtc_all_event_off,
                                                       xsp_dtc_all_event_grad,
                                                       xsp_dtc_all_event_rate_off,
                                                       xsp_dtc_all_event_rate_grad,
                                                       xsp_dtc_in_window_off,
                                                       xsp_dtc_in_window_grad,
                                                       xsp_dtc_in_window_rate_off,
                                                       xsp_dtc_in_window_rate_grad);
    if (xsp_status < XSP3_OK) {
      checkErrorCode("xsp3_setDeadtimeCorrectionParameters", xsp_status);
      status = XSP_STATUS_ERROR;
    }
  }
  return status;
}

int LibXspressWrapper::setTriggerMode()
{
  int status = XSP_STATUS_OK;
  Xsp3Timing xsp_trigger_mode = {0};
  int itfg_trig_mode;
  int xsp_status = XSP3_OK;

  LOG4CXX_DEBUG_LEVEL(1, logger_, "Set Trigger Mode.");  

  status = mapTimeFrameSource(&xsp_trigger_mode, &itfg_trig_mode);
  if (xsp_trigger_mode.t_src == XSP3_GTIMA_SRC_INTERNAL) {
    xsp_status = xsp3_itfg_setup(xsp_handle_, 
                                 0,
                                 xsp_frames_,
                                 (u_int32_t)floor(xsp_exposure_time_ / xsp_clock_period_ + 0.5),
                                 itfg_trig_mode,
                                 XSP3_ITFG_GAP_MODE_1US);
    if (xsp_status != XSP3_OK) {
      checkErrorCode("xsp3_itfg_setup", xsp_status);
      status = XSP_STATUS_ERROR;
    }
  }
  if (status != XSP_STATUS_ERROR){
    xsp_status = xsp3_set_timing(xsp_handle_, &xsp_trigger_mode);
    if (xsp_status != XSP3_OK) {
      checkErrorCode("xsp3_set_timing", xsp_status);
      status = XSP_STATUS_ERROR;
    }
  }
  return status;
}

/**
 * Function to map the database timeframe source
 * value to the macros defined by the API.
 * @param mode The database value
 * @param apiMode This returns the correct value for the API
 * @return asynStatus
 */
int LibXspressWrapper::mapTimeFrameSource(Xsp3Timing *api_mode, int *api_itfg_mode)
{
  int status = XSP_STATUS_OK;

  *api_itfg_mode = -1;

  switch (xsp_trigger_mode_) {
    case TM_SOFTWARE_START_STOP:
      api_mode->t_src = XSP3_GTIMA_SRC_FIXED;
      break;
    case TM_SOFTWARE:
      api_mode->t_src = XSP3_GTIMA_SRC_INTERNAL;
      *api_itfg_mode = XSP3_ITFG_TRIG_MODE_SOFTWARE;
      break;
    case TM_TTL_RISING_EDGE:
      api_mode->t_src = XSP3_GTIMA_SRC_INTERNAL;
      *api_itfg_mode = XSP3_ITFG_TRIG_MODE_HARDWARE;
      break;
    case TM_BURST:
      api_mode->t_src = XSP3_GTIMA_SRC_INTERNAL;
      *api_itfg_mode = XSP3_ITFG_TRIG_MODE_BURST;
      break;
    case TM_IDC:
      api_mode->t_src = XSP3_GTIMA_SRC_IDC;
      break;
    case TM_TTL_VETO_ONLY:
      api_mode->t_src = XSP3_GTIMA_SRC_TTL_VETO_ONLY;
      break;
    case TM_TTL_BOTH:
      api_mode->t_src = XSP3_GTIMA_SRC_TTL_BOTH;
      break;
    case TM_LVDS_VETO_ONLY:
      api_mode->t_src = XSP3_GTIMA_SRC_LVDS_VETO_ONLY;
      break;
    case TM_LVDS_BOTH:
      api_mode->t_src = XSP3_GTIMA_SRC_LVDS_BOTH;
      break;

    default:
      LOG4CXX_ERROR(logger_, "Mapping an unknown timeframe source. mode: " << xsp_trigger_mode_);
      status = XSP_STATUS_ERROR;
      break;
  }

  if (xsp_invert_f0_){
    api_mode->inv_f0 = 1;
  }
  if (xsp_invert_veto_){
    api_mode->inv_veto = 1;
  }
  api_mode->debounce = xsp_debounce_;

  return status;
}

/**
 * Function to return the current number of frames read
 * @param out number of frames read
 * @return status
 */
int LibXspressWrapper::getNumFramesRead(int64_t *furthest_frame)
{
  Xsp3ErrFlag flags;
  int xsp_status = xsp3_scaler_check_progress_details(xsp_handle_, &flags, 0, furthest_frame);
  if (xsp_status < XSP3_OK) {
    checkErrorCode("xsp3_dma_check_desc", xsp_status);
  }
  return xsp_status;
}

void LibXspressWrapper::setXspNumCards(int num_cards)
{
  xsp_num_cards_ = num_cards;
}

int LibXspressWrapper::getXspNumCards()
{
  return xsp_num_cards_;
}

void LibXspressWrapper::setXspNumTf(int num_tf)
{
  xsp_num_tf_ = num_tf;
}

int LibXspressWrapper::getXspNumTf()
{
  return xsp_num_tf_;
}

void LibXspressWrapper::setXspBaseIP(const std::string& address)
{
  xsp_base_IP_ = address;
}

std::string LibXspressWrapper::getXspBaseIP()
{
  return xsp_base_IP_;
}

void LibXspressWrapper::setXspMaxChannels(int max_channels)
{
  xsp_max_channels_ = max_channels;

  // Re initialise the DTC vectors based on the maximum channels
  pDTCi_.clear();
  pDTCi_.resize(XSP3_NUM_DTC_INT_PARAMS * xsp_max_channels_);
  pDTCd_.clear();
  pDTCd_.resize(XSP3_NUM_DTC_FLOAT_PARAMS * xsp_max_channels_);
}

int LibXspressWrapper::getXspMaxChannels()
{
  return xsp_max_channels_;
}

void LibXspressWrapper::setXspDebug(int debug)
{
  xsp_debug_ = debug;
}

int LibXspressWrapper::getXspDebug()
{
  return xsp_debug_;
}

void LibXspressWrapper::setXspConfigPath(const std::string& config_path)
{
  xsp_config_path_ = config_path;
}

std::string LibXspressWrapper::getXspConfigPath()
{
  return xsp_config_path_;
}

void LibXspressWrapper::setXspConfigSavePath(const std::string& config_save_path)
{
  xsp_config_save_path_ = config_save_path;
}

std::string LibXspressWrapper::getXspConfigSavePath()
{
  return xsp_config_save_path_;
}

void LibXspressWrapper::setXspUseResgrades(bool use)
{
  xsp_use_resgrades_ = use;
}

bool LibXspressWrapper::getXspUseResgrades()
{
  return xsp_use_resgrades_;
}

void LibXspressWrapper::setXspRunFlags(int flags)
{
  xsp_run_flags_ = flags;
}

int LibXspressWrapper::getXspRunFlags()
{
  return xsp_run_flags_;
}

void LibXspressWrapper::setXspDTCEnergy(double energy)
{
  xsp_dtc_energy_ = energy;
}

double LibXspressWrapper::getXspDTCEnergy()
{
  return xsp_dtc_energy_;
}

void LibXspressWrapper::setXspTriggerMode(int mode)
{
  xsp_trigger_mode_ = mode;
}

int LibXspressWrapper::getXspTriggerMode()
{
  return xsp_trigger_mode_;
}

void LibXspressWrapper::setXspInvertF0(int invert_f0)
{
  xsp_invert_f0_ = invert_f0;
}

int LibXspressWrapper::getXspInvertF0()
{
  return xsp_invert_f0_;
}

void LibXspressWrapper::setXspInvertVeto(int invert_veto)
{
  xsp_invert_veto_ = invert_veto;
}

int LibXspressWrapper::getXspInvertVeto()
{
  return xsp_invert_veto_;
}

void LibXspressWrapper::setXspDebounce(int debounce)
{
  xsp_debounce_ = debounce;
}

int LibXspressWrapper::getXspDebounce()
{
  return xsp_debounce_;
}

void LibXspressWrapper::setXspExposureTime(double exposure)
{
  xsp_exposure_time_ = exposure;
}

double LibXspressWrapper::getXspExposureTime()
{
  return xsp_exposure_time_;
}

void LibXspressWrapper::setXspFrames(int frames)
{
  xsp_frames_ = frames;
}

int LibXspressWrapper::getXspFrames()
{
  return xsp_frames_;
}

} /* namespace Xspress */
