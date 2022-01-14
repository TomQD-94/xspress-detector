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

#define XSPRESS_RX_BUFF_LWORDS 1100
#define XSPRESS_RX_HEADER_LWORDS 1

#define XSP_SOF_GET_FRAME(x)     (((x)>>0)&0xFFFFFF)    // Get time frame from first (header) word
#define XSP_SOF_GET_PREV_TIME(x) (((x)>>24)&0xFFFFFFFF) // Get total integration time from previous time frame from first (header) word
#define XSP_SOF_GET_CHAN(x)      (((x)>>60)&0xF)        // Get channel number from first (header) word


#define XSP_MASK_END_OF_FRAME    ((u_int64_t)1<<59)     // Mask for End of Frame Marker.


#define XSP_TRAILER_LWORDS       2
#define XSP_10GTX_SOF            0x80000000
#define XSP_10GTX_EOF            0x40000000
#define XSP_10GTX_PAD            0x20000000
#define XSP_10GTX_PACKET_MASK    0x0FFFFFFF
#define XSP_10GTX_TIMEOUT        30


namespace Xspress
{
  static const size_t xspress_packet_size        = XSPRESS_RX_BUFF_LWORDS * sizeof(u_int64_t);
  static const size_t frame_payload_size         = xspress_packet_size;
  static const size_t packet_header_size         = XSPRESS_RX_HEADER_LWORDS * sizeof(u_int64_t);

  typedef struct
  {
    uint32_t packet_size;
    uint64_t channel;
  } ListFrameHeader;


  static const size_t total_frame_size           = frame_payload_size + sizeof(Xspress::ListFrameHeader);

}
#endif //XSPRESS_DEFINITIONS_H
