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
        if (current_frame_buffer_id_ != -1){
          ready_callback_(current_frame_buffer_id_, current_frame_number_);
        }
        return FrameDecoder::FrameReceiveStateComplete;
    }

    void XspressFrameDecoder::frame_meta_data(int meta)
    {
      // end of message bit
      if (meta & 1) {
        current_frame_buffer_id_ = -1;
      }
    }

    void XspressFrameDecoder::monitor_buffers(void) {
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Empty: " << empty_buffer_queue_.size() << " Dropped: " << frames_dropped_);
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
