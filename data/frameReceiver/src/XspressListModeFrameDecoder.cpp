#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "XspressListModeFrameDecoder.h"

// Port on xspress to send ACK packets to
#define XSPRESS_ACK_PORT 30124
// Size of ACK packet (int32 words)
#define XSPRESS_ACK_SIZE 6
// Initialisation time (us).  For this duration after init packets will be ignored
#define XSPRESS_INIT_TIME 1000000

namespace FrameReceiver {

    XspressListModeFrameDecoder::XspressListModeFrameDecoder() : FrameDecoderUDP(),
      current_frame_buffer_(NULL),
      current_frame_number_(0),
      dropping_frame_data_(false),
      current_frame_buffer_id_(-1),
      current_state(WAITING_FOR_HEADER),
      frames_dropped_(0),
      server_socket_(0),
      initialising_(true)
    {
      // Allocate memory for the dropped frames buffer
      current_raw_packet_header_.reset(new uint8_t[Xspress::packet_header_size]);
      dropped_frame_buffer_.reset(new uint8_t[get_frame_buffer_size()]);

      // Add channel to IP mapping
      channel_map_["192.168.0.66"] = 0;
      channel_map_["192.168.0.70"] = 10;
      channel_map_["192.168.0.74"] = 20;
      channel_map_["192.168.0.78"] = 30;
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
        gettime(&init_time_);
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
      if (initialising_){
        // For the initialisation duration ignore incoming spurious packets to allow time for the FP 
        // applications to initialise
        struct timespec current_time;
        gettime(&current_time);
        uint64_t ts = elapsed_us(init_time_, current_time);
        if (ts > XSPRESS_INIT_TIME){
          // Initialise time has elapsed, we are no longer initialising
          initialising_ = false;
        } else {
          LOG4CXX_DEBUG_LEVEL(1, logger_, "Unexpected packet during initialisation, dropping...");
        }
      }

      if (current_frame_buffer_id_ == -1){
        if (empty_buffer_queue_.empty() || initialising_){
          current_frame_buffer_ = dropped_frame_buffer_.get();
          if (!dropping_frame_data_){
            uint64_t *lptr = (uint64_t *)get_packet_header_buffer();
            if (!initialising_){
              LOG4CXX_ERROR(logger_, "Time Frame: " << XSP_SOF_GET_FRAME(*lptr) << " received but no free buffers available. Dropping packet");
            }
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
        // Initialise frame header
        current_frame_header_ = reinterpret_cast<Xspress::ListFrameHeader*>(current_frame_buffer_);
        current_frame_header_->packets_received = 0;
      }
    }

    void* XspressListModeFrameDecoder::get_next_payload_buffer(void) const
    {
      uint8_t* next_receive_location =
        reinterpret_cast<uint8_t*>(current_frame_buffer_)
        + get_frame_header_size()
        + (get_next_payload_size() * current_frame_header_->packets_received)
        + get_packet_header_size();

      return reinterpret_cast<void*>(next_receive_location);
    }

    size_t XspressListModeFrameDecoder::get_next_payload_size(void) const
    {
      return Xspress::xspress_packet_size;
    }

    FrameDecoder::FrameReceiveState XspressListModeFrameDecoder::process_packet(size_t bytes_received, int port, struct sockaddr_in* from_addr)
    {
      FrameDecoder::FrameReceiveState frame_state = FrameDecoder::FrameReceiveStateIncomplete;
      bool end_of_frame = false;
      uint64_t *lptr = (uint64_t *)get_packet_header_buffer();
      uint64_t chan_of_card = XSP_SOF_GET_CHAN(*lptr);
      char *ip = inet_ntoa(from_addr->sin_addr);
      std::string str_ip(ip);
      uint64_t channel = channel_map_[str_ip] + chan_of_card;

      // We need to copy the 2 word packet header into the frame at the correct location
      uint8_t* packet_header_location = reinterpret_cast<uint8_t*>(current_frame_buffer_)
                                  			+ get_frame_header_size()
			                                  + (get_next_payload_size() * current_frame_header_->packets_received);
      memcpy(packet_header_location, get_packet_header_buffer(), get_packet_header_size());

      if (*lptr & XSP_MASK_END_OF_FRAME){
        // Acknowledge packet to send if get a end of frame marker.
        end_of_frame = true;
        uint32_t tbuff[XSPRESS_ACK_SIZE];
        tbuff[0] = 0;
        tbuff[1] = XSP_10GTX_SOF | XSP_10GTX_EOF; // Single packet frame
        tbuff[2] = XSP_SOF_GET_FRAME(*lptr); // Frame
        tbuff[3] = chan_of_card;
        tbuff[4] = 0; // Dummy data sent with EOF
        tbuff[5] = 0; // Dummy data sent with EOF
        LOG4CXX_DEBUG_LEVEL(1, logger_, "Sending ack for channel: " << chan_of_card << " socket: " << ip << ":" << port);

        // If the reply socket has not been created yet then do so now
        if (server_socket_ == 0){
          server_address_.sin_family = AF_INET;
          (server_address_.sin_addr).s_addr = inet_addr(ip);
          server_address_.sin_port = htons(XSPRESS_ACK_PORT);

          socklen_t addr_size = sizeof(server_address_);

          server_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
          connect(server_socket_, (struct sockaddr *)&server_address_, addr_size);
        }
        // Send ack to ACK_PORT, same address EOF packet came from.
        // Hardware set to filter on destination port which is the same on the FEM for all channels.
        write(server_socket_, tbuff, sizeof(u_int32_t)*XSPRESS_ACK_SIZE);
      }

      LOG4CXX_DEBUG_LEVEL(3, logger_, "Packet => channel_of_card: " << chan_of_card << " channel: " << channel << " socket: " << ip << ":" << port);
      // Set the size of the packet in the frame header
      current_frame_header_->packet_headers[current_frame_header_->packets_received].packet_size = bytes_received;
      // Set the channel number in the frame header
      current_frame_header_->packet_headers[current_frame_header_->packets_received].channel = channel;

      // Increment the number of packets received for this frame
      if (!dropping_frame_data_) {
        current_frame_header_->packets_received++;
        LOG4CXX_DEBUG_LEVEL(2, logger_, "  Packet count: " << current_frame_header_->packets_received);
      }

      // Check we are not dropping data for this frame
      if (!dropping_frame_data_){
        if (current_frame_header_->packets_received == XSP_PACKETS_PER_FRAME || end_of_frame){
          // Notify main thread that frame is ready
          ready_callback_(current_frame_buffer_id_, current_frame_number_);
          current_frame_number_++;
          current_frame_buffer_id_ = -1;
          // Set frame state accordingly
          frame_state = FrameDecoder::FrameReceiveStateComplete;
        }
      }
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

}
