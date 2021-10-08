/*
 * LibXspressWrapper.h
 *
 *  Created on: 22 Sep 2021
 *      Author: Diamond Light Source
 */

#ifndef LibXspressWrapper_H_
#define LibXspressWrapper_H_

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/helpers/exception.h>

#include "logging.h"
#include "xspress3.h"

using namespace log4cxx;
using namespace log4cxx::helpers;

#define XSP_STATUS_OK     0
#define XSP_STATUS_ERROR -1

#define XSP3_NUM_DTC_FLOAT_PARAMS           8
#define XSP3_NUM_DTC_INT_PARAMS             1
#define XSP3_DTC_FLAGS                      0
#define XSP3_DTC_AEO                        0
#define XSP3_DTC_AEG                        1
#define XSP3_DTC_AERO                       2
#define XSP3_DTC_AERG                       3
#define XSP3_DTC_IWO                        4
#define XSP3_DTC_IWG                        5
#define XSP3_DTC_IWRO                       6
#define XSP3_DTC_IWRG                       7

// Trigger mode enum valeus
#define TM_SOFTWARE                         0
#define TM_TTL_RISING_EDGE                  1
#define TM_BURST                            2
#define TM_TTL_VETO_ONLY                    3
#define TM_SOFTWARE_START_STOP              4
#define TM_IDC                              5
#define TM_TTL_BOTH                         6
#define TM_LVDS_VETO_ONLY                   7
#define TM_LVDS_BOTH                        8

namespace Xspress
{

/**
 * The LibXspressWrapper class provides an OO implementation based around the C
 * libxspress library.  This class is designed to abstract specific libxspress
 * calls.
 * 
 * This class provides a control interface and a mechanism for reading out
 * data.  The data interface is optional in case another method is used to 
 * collect data directly from the detector hardware.
 */
class LibXspressWrapper
{
public:
  LibXspressWrapper(bool simulation=false);
  virtual ~LibXspressWrapper();
  void setErrorString(const std::string& error);
  std::string getErrorString();
  void checkErrorCode(const std::string& prefix, int code);
  int connect();
  int checkConnected();
  int saveSettings();
  int restoreSettings();
  int readSCAParams();
  int readDTCParams();
  int writeDTCParams();
  int setTriggerMode();
  int mapTimeFrameSource(Xsp3Timing *api_mode, int *api_itfg_mode);
  int getNumFramesRead(int64_t *furthest_frame);

  // Getter and Setters
  void setXspNumCards(int num_cards);
  int getXspNumCards();
  void setXspNumTf(int num_tf);
  int getXspNumTf();
  void setXspBaseIP(const std::string& address);
  std::string getXspBaseIP();
  void setXspMaxChannels(int max_channels);
  int getXspMaxChannels();
  void setXspDebug(int debug);
  int getXspDebug();
  void setXspConfigPath(const std::string& config_path);
  std::string getXspConfigPath();
  void setXspConfigSavePath(const std::string& config_save_path);
  std::string getXspConfigSavePath();
  void setXspUseResgrades(bool use);
  bool getXspUseResgrades();
  void setXspRunFlags(int flags);
  int getXspRunFlags();
  void setXspDTCEnergy(double energy);
  double getXspDTCEnergy();
  void setXspTriggerMode(int mode);
  int getXspTriggerMode();
  void setXspInvertF0(int invert_f0);
  int getXspInvertF0();
  void setXspInvertVeto(int invert_veto);
  int getXspInvertVeto();
  void setXspDebounce(int debounce);
  int getXspDebounce();
  void setXspExposureTime(double exposure);
  double getXspExposureTime();
  void setXspFrames(int frames);
  int getXspFrames();
  
  static const int runFlag_MCA_SPECTRA_;
  static const int runFlag_SCALERS_ONLY_;
  static const int runFlag_PLAYB_MCA_SPECTRA_;


private:
  /** Simulation flag for this wrapper */
  bool                          simulated_;

  /** Connected flag */
  bool                          connected_;
  /** Reconnection required flag */
  bool                          reconnect_required_;
  /** Handle used by the libxspress library */
  int                           xsp_handle_;
  /** Number of XSPRESS cards */
  int                           xsp_num_cards_;
  /** Number of 4096 energy bin spectra timeframes */
  int                           xsp_num_tf_;
  /** Base IP address */
  std::string                   xsp_base_IP_;
  /** Set the maximum number of channels */
  int                           xsp_max_channels_;
  /** Enable debug messages */
  int                           xsp_debug_;
  /** Path to Xspress configuration files */
  std::string                   xsp_config_path_;
  /** Path to Xspress configuration files */
  std::string                   xsp_config_save_path_;
  /** Use resgrades? */
  bool                          xsp_use_resgrades_;
  /** Number of auxiliary data items */
  bool                          xsp_num_aux_data_;
  /** Number of auxiliary data items */
  int                           xsp_run_flags_;
  /** Have the DTC params been updated */
  bool                          xsp_dtc_params_updated_;
  /** DTC energy */
  double                        xsp_dtc_energy_;
  /** Clock period */
  double                        xsp_clock_period_;
  /** Trigger mode */
  int                           xsp_trigger_mode_;
  /** Invert f0 */
  int                           xsp_invert_f0_;
  /** Invert veto */
  int                           xsp_invert_veto_;
  /** Debounce */
  int                           xsp_debounce_;
  /** Exposure time */
  double                        xsp_exposure_time_;
  /** Number of frames */
  int                           xsp_frames_;

  std::vector<uint32_t>         xsp_chan_sca5_low_lim_;
  std::vector<uint32_t>         xsp_chan_sca5_high_lim_;
  std::vector<uint32_t>         xsp_chan_sca6_low_lim_;
  std::vector<uint32_t>         xsp_chan_sca6_high_lim_;

  std::vector<uint32_t>         xsp_chan_sca4_threshold_;


  std::vector<int>              pDTCi_;
  std::vector<double>           pDTCd_;
  
  std::vector<std::pair <int, int> > cardChanMap;

  /** Pointer to the logging facility */
  log4cxx::LoggerPtr            logger_;

  /** Last error string description */
  std::string                   error_string_;
};

} /* namespace Xspress */

#endif /* LibXspressWrapper_H_ */
