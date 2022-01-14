#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "XspressListModeFrameDecoder.h"

namespace FrameReceiver {

    XspressListModeFrameDecoder::XspressListModeFrameDecoder() : FrameDecoderUDP(),
      current_frame_buffer_(NULL),
      current_frame_number_(0),
      dropping_frame_data_(false),
      current_frame_buffer_id_(-1),
      current_state(WAITING_FOR_HEADER),
      frames_dropped_(0),
      numChannels(8),
      numEnergy(4096),
      numAux(1),
      currentChannel(0) 
    {
      // Allocate memory for the dropped frames buffer
      current_raw_packet_header_.reset(new uint8_t[Xspress::packet_header_size]);
      dropped_frame_buffer_.reset(new uint8_t[get_frame_buffer_size()]);

      // Add channel to IP mapping
      channel_map_["192.168.0.66"] = 0;
      channel_map_["192.168.0.70"] = 9;
      channel_map_["192.168.0.74"] = 18;
      channel_map_["192.168.0.78"] = 27;
    }

    XspressListModeFrameDecoder::~XspressListModeFrameDecoder() {
    }

    // Version functions
    int XspressListModeFrameDecoder::get_version_major() {
        return XSPRESS_DETECTOR_VERSION_MAJOR;
    }

    int XspressListModeFrameDecoder::get_version_minor() {
        return XSPRESS_DETECTOR_VERSION_MINOR;
    }

    int XspressListModeFrameDecoder::get_version_patch() {
        return XSPRESS_DETECTOR_VERSION_PATCH;
    }

    std::string XspressListModeFrameDecoder::get_version_short() {
        return XSPRESS_DETECTOR_VERSION_STR_SHORT;
    }

    std::string XspressListModeFrameDecoder::get_version_long() {
        return XSPRESS_DETECTOR_VERSION_STR;
    }

    void XspressListModeFrameDecoder::init(LoggerPtr &logger, OdinData::IpcMessage &config_msg) {
        this->logger_ = Logger::getLogger("FR.XspressListModeFrameDecoder");
        this->logger_->setLevel(Level::getAll());
        FrameDecoder::init(logger, config_msg);
        LOG4CXX_INFO(logger_, "Xspress list mode frame decoder init complete");
    }

    void XspressListModeFrameDecoder::request_configuration(const std::string param_prefix, OdinData::IpcMessage& config_reply)
    {

    }

    const size_t XspressListModeFrameDecoder::get_frame_buffer_size(void) const
    {
      return Xspress::total_frame_size;
    }

    const size_t XspressListModeFrameDecoder::get_frame_header_size(void) const
    {
      return sizeof(Xspress::ListFrameHeader);
    }

    const size_t XspressListModeFrameDecoder::get_packet_header_size(void) const
    {
      return Xspress::packet_header_size;
    }

    void XspressListModeFrameDecoder::process_packet_header(size_t bytes_received, int port, struct sockaddr_in* from_addr)
    {
      if (empty_buffer_queue_.empty()){
        current_frame_buffer_ = dropped_frame_buffer_.get();
        if (!dropping_frame_data_){
          uint64_t *lptr = (uint64_t *)get_packet_header_buffer();
          LOG4CXX_ERROR(logger_, "Time Frame: " << XSP_SOF_GET_FRAME(*lptr) << " received but no free buffers available. Dropping packet");
          dropping_frame_data_ = true;
        }
      } else {
        current_frame_buffer_id_ = empty_buffer_queue_.front();
        empty_buffer_queue_.pop();
        current_frame_buffer_ = buffer_manager_->get_buffer_address(current_frame_buffer_id_);

        if (dropping_frame_data_){
          dropping_frame_data_ = false;
          LOG4CXX_DEBUG_LEVEL(2, logger_, "Free buffers are now available, allocating frame buffer ID " << current_frame_buffer_id_);
        }
      }

      current_frame_header_ = reinterpret_cast<Xspress::ListFrameHeader*>(current_frame_buffer_);
      //LOG4CXX_ERROR(logger_, "In process packet header");
      //std::stringstream stream;
      //stream << std::hex << *lptr;
      //LOG4CXX_ERROR(logger_, "Time Frame: " << XSP_SOF_GET_FRAME(*lptr));
      //LOG4CXX_ERROR(logger_, "Previous Time: " << XSP_SOF_GET_PREV_TIME(*lptr));
      //LOG4CXX_ERROR(logger_, "Channel: " << XSP_SOF_GET_CHAN(*lptr));
    }

    void* XspressListModeFrameDecoder::get_next_payload_buffer(void) const
    {
      uint8_t* next_receive_location =
        reinterpret_cast<uint8_t*>(current_frame_buffer_)
		+ get_frame_header_size()
		+ get_packet_header_size();

      return reinterpret_cast<void*>(next_receive_location);
    }

    size_t XspressListModeFrameDecoder::get_next_payload_size(void) const
    {
      return Xspress::xspress_packet_size;
    }

    FrameDecoder::FrameReceiveState XspressListModeFrameDecoder::process_packet(size_t bytes_received, int socket, int port, struct sockaddr_in* from_addr)
    {
      uint64_t *lptr = (uint64_t *)get_packet_header_buffer();
      uint64_t chan_of_card = XSP_SOF_GET_CHAN(*lptr);
      char *ip = inet_ntoa(from_addr->sin_addr);
      std::string str_ip(ip);
      uint64_t channel = channel_map_[str_ip] + chan_of_card;
      LOG4CXX_ERROR(logger_, "Packet received from IP address: " << ip);
      LOG4CXX_ERROR(logger_, "XSP_SOF_GET_FRAME: " << XSP_SOF_GET_FRAME(*lptr));
      LOG4CXX_ERROR(logger_, "Channel: " << channel);

      if (*lptr & XSP_MASK_END_OF_FRAME){
        // Acknowledge packet to send if get a end of frame marker.
        uint32_t tbuff[6];
        tbuff[0] = 0;
        tbuff[1] = XSP_10GTX_SOF | XSP_10GTX_EOF; // Single packet frame
        tbuff[2] = XSP_SOF_GET_FRAME(*lptr); // Frame
        tbuff[3] = chan_of_card;
        tbuff[4] = 0; // Dummy data sent with EOF
        tbuff[5] = 0; // Dummy data sent with EOF
        LOG4CXX_ERROR(logger_, "Sending ack for channel: " << chan_of_card);
        write(socket, tbuff, sizeof(u_int32_t)*6); // Send ack to same port data came from. hardware set to filter on destination port which is the same om the FEM for all channels.
      }

      // Set the size of the packet in the frame header
      current_frame_header_->packet_size = bytes_received;
      // Set the channel number in the frame header
      current_frame_header_->channel = channel;

      // We need to copy the single word packet header into the frame at the correct location
      uint8_t* packet_header_location =
                reinterpret_cast<uint8_t*>(current_frame_buffer_) +
                get_frame_header_size();
      memcpy(packet_header_location, get_packet_header_buffer(), get_packet_header_size());

      // Check we are not dropping data for this frame
      if (!dropping_frame_data_){
  			// Notify main thread that frame is ready
	  		ready_callback_(current_frame_buffer_id_, current_frame_number_);
        current_frame_number_++;
      }

      // Set frame state accordingly
	    FrameDecoder::FrameReceiveState frame_state = FrameDecoder::FrameReceiveStateComplete;
      return frame_state;
    }

    void XspressListModeFrameDecoder::monitor_buffers(void)
    {

    }

    void XspressListModeFrameDecoder::get_status(const std::string param_prefix, OdinData::IpcMessage& status_msg)
    {

    }

    void XspressListModeFrameDecoder::reset_statistics(void)
    {

    }

    void* XspressListModeFrameDecoder::get_packet_header_buffer(void)
    {
      return current_raw_packet_header_.get();
    }

    //uint32_t get_frame_number(void) const
    //{
    //
    //}

    //uint32_t get_packet_number(void) const
    //{
    //
    //}

}
