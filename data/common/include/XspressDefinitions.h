#ifndef XSPRESS_DEFINITIONS_H
#define XSPRESS_DEFINITIONS_H

#include "version.h"
#include "Frame.h"
#include "xspress3Definitions.h"
#include "xspress3MagicNumbers.h"

#define XSPRESS_DATA_TYPE raw_32bit // 1 UINT16
#define XSP3_SW_NUM_SCALERS 9

enum XspressState {
  WAITING_FOR_HEADER=0,
  WAITING_FOR_MCA,
  WAITING_FOR_SCA
};

#endif //XSPRESS_DEFINITIONS_H
