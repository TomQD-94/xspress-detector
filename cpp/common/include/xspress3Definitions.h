//
// Created by myr45768 on 13/11/2019.
//

#ifndef _XSPRESS3DEFINITIONS_EPICS_H
#define _XSPRESS3DEFINITIONS_EPICS_H

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

typedef struct
{
  uint32_t frame_number;
  uint32_t num_energy_bins;
  uint32_t num_aux;
  uint32_t num_channels;
  uint32_t num_scalars;
  uint32_t first_channel;
  //double dead_time_energy;
  //double clock_period;
} FrameHeader;

#endif //_XSPRESS3DEFINITIONS_EPICS_H
