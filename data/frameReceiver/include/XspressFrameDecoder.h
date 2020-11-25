#ifndef SRC_XSPRESSDECODER_H
#define SRC_XSPRESSDECODER_H

#include <boost/shared_ptr.hpp>
#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/helpers/exception.h>
using namespace log4cxx;
using namespace log4cxx::helpers;

#include "FrameDecoderZMQ.h"
#include "XspressDefinitions.h"
#include "IpcMessage.h"

namespace FrameReceiver {

  class XspressFrameDecoder : public FrameDecoderZMQ {

  public:
    XspressFrameDecoder();

    ~XspressFrameDecoder();

    // version related functions
    int get_version_major();

    int get_version_minor();

    int get_version_patch();

    std::string get_version_short();

    std::string get_version_long();

    // decoder interface
    void init(LoggerPtr& logger, OdinData::IpcMessage& config_msg);

    const size_t get_frame_buffer_size(void) const;

    const size_t get_frame_header_size(void) const;

    // ZMQ decoder interface
    void *get_next_message_buffer(void);

    FrameReceiveState process_message(size_t bytes_received);

    void frame_meta_data(int meta);

    void monitor_buffers(void);

    void get_status(const std::string param_prefix, OdinData::IpcMessage &status_msg);

  private:
    void *current_frame_buffer_;
    int32_t current_frame_buffer_id_;
    uint32_t current_frame_number_;
    enum XspressState current_state;
    // statistics
    unsigned int frames_dropped_;
    size_t numChannels;
    uint32_t numEnergy;
    uint32_t numAux;
    size_t currentChannel;

  };

}

#endif //SRC_XSPRESSDECODER_H
