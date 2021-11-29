#include <iostream>
#include "XspressFrameDecoder.h"

namespace FrameReceiver {

    XspressFrameDecoder::XspressFrameDecoder() : FrameDecoderZMQ(), current_frame_buffer_(NULL), current_frame_number_(0),
                                         current_frame_buffer_id_(-1), current_state(WAITING_FOR_HEADER),
                                         frames_dropped_(0), numChannels(8), numEnergy(4096), numAux(1), currentChannel(0) 
    {
      // Allocate memory for the dropped frames buffer
      dropped_frame_buffer_ = malloc(get_frame_buffer_size());
    }

    XspressFrameDecoder::~XspressFrameDecoder() {
    }

    void XspressFrameDecoder::init(LoggerPtr &logger, OdinData::IpcMessage &config_msg) {
        this->logger_ = Logger::getLogger("FR.XspressFrameDecoder");
        this->logger_->setLevel(Level::getAll());
        FrameDecoder::init(logger, config_msg);
        LOG4CXX_INFO(logger_, "Xspress frame decoder init complete");
    }

    void *XspressFrameDecoder::get_next_message_buffer(void) {
        if (__builtin_expect(empty_buffer_queue_.empty(), false)) {
          // dropped for not having buffers available
          frames_dropped_++;
          LOG4CXX_ERROR(logger_, "XspressFrameDecoder: Dropped " << frames_dropped_ << " frames");
          current_frame_buffer_ = dropped_frame_buffer_;
          // use last valid buffer
        } else if (current_frame_buffer_id_ == -1) {
          current_frame_buffer_id_ = empty_buffer_queue_.front();
          empty_buffer_queue_.pop();
          current_frame_buffer_ = buffer_manager_->get_buffer_address(current_frame_buffer_id_);
        }
        return current_frame_buffer_;
    }

    FrameDecoder::FrameReceiveState XspressFrameDecoder::process_message(size_t bytes_received)
    {
        FrameHeader *header_ = reinterpret_cast<FrameHeader*> (current_frame_buffer_);
        current_frame_number_ = header_->frame_number;
//        LOG4CXX_INFO(logger_, "Xspress message for frame [" << current_frame_number_ << "] received with " << bytes_received << " bytes");
        if (current_frame_buffer_id_ != -1){
          ready_callback_(current_frame_buffer_id_, current_frame_number_);
        }
        return FrameDecoder::FrameReceiveStateComplete;

/*

        uint32_t *ptr = (uint32_t *)current_frame_buffer_;
        LOG4CXX_INFO(logger_, "Frame number[" << ptr[0] << 
                              "] Num spectra[" << ptr[1] << 
                              "] Num aux[" << ptr[2] <<
                              "] Num channels[" << ptr[3] <<
                              "] Num scalars[" << ptr[4] << "]");

        LOG4CXX_INFO(logger_, "Scalar 1[" << ptr[5] << 
                              "] Scalar 2[" << ptr[6] << 
                              "] Scalar 3[" << ptr[7] << 
                              "] Scalar 4[" << ptr[8] << 
                              "] Scalar 5[" << ptr[9] << 
                              "] Scalar 6[" << ptr[10] << 
                              "] Scalar 7[" << ptr[11] << 
                              "] Scalar 8[" << ptr[12] << 
                              "] Scalar 9[" << ptr[13] << "]");

        char *t_ptr = (char *)current_frame_buffer_;
        t_ptr += ((5*4) + (9*9*4));
        double *dtc_ptr = (double *)t_ptr;
        LOG4CXX_INFO(logger_, "DTC factor[" << dtc_ptr[0] << 
                              "] Input Estimate[" << dtc_ptr[1] << "]");


        current_frame_number_ = ptr[0];

        switch (current_state) {
            case WAITING_FOR_HEADER:
              {
                FrameHeader *header_ = reinterpret_cast<FrameHeader*> (current_frame_buffer_);
                numEnergy = header_->numEnergy;
                numAux = header_->numAux;
                numChannels = header_->numChannels;
                current_frame_number_ = header_->frameNumber;
                currentChannel = 0;
                current_state = WAITING_FOR_MCA;
                return FrameDecoder::FrameReceiveStateIncomplete;
              }
            case WAITING_FOR_MCA:
              {
                current_state = (++currentChannel) >= numChannels ? WAITING_FOR_SCA : WAITING_FOR_MCA;
                return FrameDecoder::FrameReceiveStateIncomplete;
              }
            case WAITING_FOR_SCA:
              {
                ready_callback_(current_frame_buffer_id_, current_frame_number_);
                return FrameDecoder::FrameReceiveStateComplete;
              }
        }
*/
    }

    void XspressFrameDecoder::frame_meta_data(int meta)
    {
      // end of message bit
      if (meta & 1) {
        current_frame_buffer_id_ = -1;
      }
    }

    void XspressFrameDecoder::monitor_buffers(void) {
      LOG4CXX_INFO(logger_, "Empty: " << empty_buffer_queue_.size() << " Dropped: " << frames_dropped_);
    }

    void XspressFrameDecoder::get_status(const std::string param_prefix, OdinData::IpcMessage &status_msg) {
        status_msg.set_param(param_prefix + "name", std::string("XspressFrameDecoder"));
        status_msg.set_param(param_prefix + "frames_dropped", frames_dropped_);
    }

    const size_t XspressFrameDecoder::get_frame_buffer_size(void) const {
        return (numChannels * ((numEnergy * numAux + XSP3_SW_NUM_SCALERS)*sizeof(uint32_t)) + sizeof(FrameHeader));
    }

    const size_t XspressFrameDecoder::get_frame_header_size(void) const {
        return 0;
    }

    // Version functions
    int XspressFrameDecoder::get_version_major() {
        return XSPRESS_DETECTOR_VERSION_MAJOR;
    }

    int XspressFrameDecoder::get_version_minor() {
        return XSPRESS_DETECTOR_VERSION_MINOR;
    }

    int XspressFrameDecoder::get_version_patch() {
        return XSPRESS_DETECTOR_VERSION_PATCH;
    }

    std::string XspressFrameDecoder::get_version_short() {
        return XSPRESS_DETECTOR_VERSION_STR_SHORT;
    }

    std::string XspressFrameDecoder::get_version_long() {
        return XSPRESS_DETECTOR_VERSION_STR;
    }

}
