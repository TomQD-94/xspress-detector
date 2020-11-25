#include <iostream>
#include "XspressFrameDecoder.h"

namespace FrameReceiver {

    XspressFrameDecoder::XspressFrameDecoder() : FrameDecoderZMQ(), current_frame_buffer_(NULL), current_frame_number_(0),
                                         current_frame_buffer_id_(-1), current_state(WAITING_FOR_HEADER),
                                         frames_dropped_(0), numChannels(8), numEnergy(4096), numAux(1), currentChannel(0) {
    }

    XspressFrameDecoder::~XspressFrameDecoder() {
    }

    void XspressFrameDecoder::init(LoggerPtr &logger, OdinData::IpcMessage &config_msg) {
        this->logger_ = Logger::getLogger("FR.XspressFrameDecoder");
        this->logger_->setLevel(Level::getAll());
        FrameDecoder::init(logger, config_msg);
        LOG4CXX_INFO(logger_, "Xspress frame decoder init called, citizens rejoice!");
    }

    void *XspressFrameDecoder::get_next_message_buffer(void) {
        if (__builtin_expect(empty_buffer_queue_.empty(), false)) {
          // dropped for not having buffers available
          frames_dropped_++;
          // use last valid buffer
          return current_frame_buffer_;
        } else if (current_frame_buffer_id_ == -1) {
          current_frame_buffer_id_ = empty_buffer_queue_.front();
          empty_buffer_queue_.pop();
          current_frame_buffer_ = buffer_manager_->get_buffer_address(current_frame_buffer_id_);
        }
        if (current_state == WAITING_FOR_HEADER) {
          return current_frame_buffer_;
        }
        return static_cast<void *>(static_cast<char *>(buffer_manager_->get_buffer_address(current_frame_buffer_id_)) + sizeof(FrameHeader) + sizeof(uint32_t)*currentChannel * numEnergy * numAux);
    }

    FrameDecoder::FrameReceiveState XspressFrameDecoder::process_message(size_t bytes_received) {
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
    }

    void XspressFrameDecoder::frame_meta_data(int meta) {
        // end of message bit
        if (meta & 1) {
            current_frame_buffer_id_ = -1;
            current_state = WAITING_FOR_HEADER;
        }
    }

    void XspressFrameDecoder::monitor_buffers(void) {

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
