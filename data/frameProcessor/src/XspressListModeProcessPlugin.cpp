//
// Created by hir12111 on 03/11/18.
//
#include <iostream>
#include "DataBlockFrame.h"
#include "XspressListModeProcessPlugin.h"
#include "FrameProcessorDefinitions.h"
#include "XspressDefinitions.h"
#include "DebugLevelLogger.h"

namespace FrameProcessor {

const std::string XspressListModeProcessPlugin::CONFIG_CHANNELS =           "channels";
const std::string XspressListModeProcessPlugin::CONFIG_RESET_ACQUISITION =  "reset";
const std::string XspressListModeProcessPlugin::CONFIG_FRAME_SIZE =         "frame_size";


XspressListModeMemoryBlock::XspressListModeMemoryBlock(const std::string& name) :
  ptr_(0),
  num_bytes_(0),
  num_words_(0),
  filled_size_(0),
  frame_count_(0)
{
  // Setup logging for the class
  logger_ = Logger::getLogger("FP.XspressListModeProcessPlugin");
  LOG4CXX_INFO(logger_, "Created XspressListModeMemoryBlock");

  name_ = name;
}

XspressListModeMemoryBlock::~XspressListModeMemoryBlock()
{
  if (ptr_){
    free(ptr_);
  }
}

void XspressListModeMemoryBlock::set_size(uint32_t bytes)
{
  num_bytes_ = bytes;
  num_words_ = num_bytes_ / sizeof(uint64_t);
  reallocate();
}

void XspressListModeMemoryBlock::reallocate()
{
  LOG4CXX_INFO(logger_, "Reallocating XspressListModeMemoryBlock to [" << num_bytes_ << "] bytes");
  if (ptr_){
    free(ptr_);
  }
  ptr_ = (char *)malloc(num_bytes_);
  reset();
}

void XspressListModeMemoryBlock::reset()
{
  memset(ptr_, 0, num_bytes_);
  filled_size_ = 0;
}

void XspressListModeMemoryBlock::reset_frame_count()
{
  frame_count_ = 0;
}

boost::shared_ptr <Frame> XspressListModeMemoryBlock::add_block(uint32_t bytes, void *ptr)
{
  boost::shared_ptr <Frame> frame;

  // Calculate the current end of data pointer location
  char *dest = (char *)ptr_;
  dest += filled_size_;
  // Set the number of packet words as a 64bit variable
  uint64_t pkt_words = (uint64_t)(bytes / sizeof(uint64_t));

  if (filled_size_ == num_bytes_){
    // Buffer is already full (this shouldn't really be possible but best to check)
    frame = this->to_frame();
  }

  // Copy the number of words into the dataset
  memcpy(dest, &pkt_words, sizeof(uint64_t));
  dest += sizeof(uint64_t);

  // Work out if adding the block will result in a full frame
  if (filled_size_ + bytes < num_bytes_){
    // We can copy the entire block into the store
    memcpy(dest, ptr, bytes);
    filled_size_ += bytes;
  } else {
    // Fill up the remainder of the block, create the frame and then copy over 
    // any remaining bytes
    uint32_t bytes_to_full = num_bytes_ - filled_size_;
    memcpy(dest, ptr, bytes_to_full);

    frame = this->to_frame();

    // Copy any remaining data
    uint32_t remaining_bytes = bytes - bytes_to_full;
    if (remaining_bytes > 0){
      dest = (char *)ptr_;
      char *src = (char *)ptr;
      src += bytes_to_full;
      memcpy(dest, src, remaining_bytes);
      filled_size_ += remaining_bytes;
    }
  }
  
  // Final check, if we have a full buffer then send it out
  if (filled_size_ == num_bytes_){
    frame = this->to_frame();
  }

  return frame;
}

boost::shared_ptr <Frame> XspressListModeMemoryBlock::to_frame()
{
  boost::shared_ptr <Frame> frame;

  // Create the frame around the current (partial) block
  dimensions_t dims;
  FrameMetaData list_metadata(frame_count_, name_, raw_64bit, "", dims);
  frame = boost::shared_ptr<Frame>(new DataBlockFrame(list_metadata, num_bytes_));
  memcpy(frame->get_data_ptr(), ptr_, num_bytes_);

  // Reset the block
  reset();

  // Add 1 to the frame count
  frame_count_++;

  return frame;
}

XspressListModeProcessPlugin::XspressListModeProcessPlugin() :
  num_channels_(0)
{
  // Setup logging for the class
  logger_ = Logger::getLogger("FP.XspressListModeProcessPlugin");
  LOG4CXX_INFO(logger_, "XspressListModeProcessPlugin version " << this->get_version_long() << " loaded");
}

XspressListModeProcessPlugin::~XspressListModeProcessPlugin()
{
  LOG4CXX_TRACE(logger_, "XspressListModeProcessPlugin destructor.");
}

void XspressListModeProcessPlugin::configure(OdinData::IpcMessage& config, OdinData::IpcMessage& reply) 
{
  if (config.has_param(XspressListModeProcessPlugin::CONFIG_CHANNELS)){
    std::stringstream ss;
    ss << "Configure process plugin for channels [";
    const rapidjson::Value& channels = config.get_param<const rapidjson::Value&>(XspressListModeProcessPlugin::CONFIG_CHANNELS);
    std::vector<uint32_t> channel_vec;
    size_t num_channels = channels.Size();
    for (size_t i = 0; i < num_channels; i++) {
      const rapidjson::Value& chan_value = channels[i];
      channel_vec.push_back(chan_value.GetInt());
      ss << chan_value.GetInt();
      if (i < num_channels - 1){
        ss << ", ";
      }
    }
    ss << "]";
    LOG4CXX_INFO(logger_, ss.str());
    this->set_channels(channel_vec);
  }

  if (config.has_param(XspressListModeProcessPlugin::CONFIG_RESET_ACQUISITION)){
    this->reset_acquisition();
  }

  if (config.has_param(XspressListModeProcessPlugin::CONFIG_FRAME_SIZE)){
    unsigned int frame_size = config.get_param<unsigned int>(XspressListModeProcessPlugin::CONFIG_FRAME_SIZE);
    this->set_frame_size(frame_size);
  }
}

// Version functions
int XspressListModeProcessPlugin::get_version_major() 
{
  return XSPRESS_DETECTOR_VERSION_MAJOR;
}

int XspressListModeProcessPlugin::get_version_minor()
{
  return XSPRESS_DETECTOR_VERSION_MINOR;
}

int XspressListModeProcessPlugin::get_version_patch()
{
  return XSPRESS_DETECTOR_VERSION_PATCH;
}

std::string XspressListModeProcessPlugin::get_version_short()
{
  return XSPRESS_DETECTOR_VERSION_STR_SHORT;
}

std::string XspressListModeProcessPlugin::get_version_long()
{
  return XSPRESS_DETECTOR_VERSION_STR;
}

void XspressListModeProcessPlugin::set_channels(std::vector<uint32_t> channels)
{
  channels_ = channels;
  num_channels_ = channels.size();
  // We must reallocate memory blocks
  setup_memory_allocation();
}

void XspressListModeProcessPlugin::reset_acquisition()
{
  LOG4CXX_INFO(logger_, "Resetting acquisition");
  std::map<uint32_t, boost::shared_ptr<XspressListModeMemoryBlock> >::iterator iter;
  for (iter = memory_ptrs_.begin(); iter != memory_ptrs_.end(); ++iter){
    iter->second->reset_frame_count();
  }
}

void XspressListModeProcessPlugin::set_frame_size(uint32_t num_bytes)
{
  LOG4CXX_INFO(logger_, "Setting frame size to " << num_bytes << " bytes");
  frame_size_bytes_ = num_bytes;
  // We must reallocate memory blocks
  setup_memory_allocation();
}

void XspressListModeProcessPlugin::setup_memory_allocation()
{
  // First clear out the memory vector emptying any blocks
  memory_ptrs_.clear();

  // Allocate large enough blocks of memory to hold list mode frames
  // Allocate one block of memory for each channel
  std::vector<uint32_t>::iterator iter;
  for (iter = channels_.begin(); iter != channels_.end(); ++iter){
    std::stringstream ss;
    ss << "raw_" << *iter;
    boost::shared_ptr<XspressListModeMemoryBlock> ptr = boost::shared_ptr<XspressListModeMemoryBlock>(new XspressListModeMemoryBlock(ss.str()));
    ptr->set_size(frame_size_bytes_);
    memory_ptrs_[*iter] = ptr;
  }
}

void XspressListModeProcessPlugin::process_frame(boost::shared_ptr <Frame> frame) 
{
  char* frame_bytes = static_cast<char *>(frame->get_data_ptr());	
  Xspress::ListFrameHeader *header = reinterpret_cast<Xspress::ListFrameHeader *>(frame_bytes);

  char *data_ptr = frame_bytes;
  data_ptr += sizeof(Xspress::ListFrameHeader);

  // Obtain the packet size and the channel number
  uint32_t pkt_size = header->packet_size;
  uint32_t channel = header->channel;

  LOG4CXX_DEBUG_LEVEL(3, logger_, "Received " << pkt_size << " bytes from channel " << channel);

  // Peek at incoming words
  uint64_t *peek_ptr = (uint64_t *)data_ptr;

  LOG4CXX_DEBUG_LEVEL(3, logger_, "Peek at first few words");
  LOG4CXX_DEBUG_LEVEL(3, logger_, " >> " << peek_ptr[0]);
  LOG4CXX_DEBUG_LEVEL(3, logger_, " >> " << peek_ptr[1]);
  LOG4CXX_DEBUG_LEVEL(3, logger_, " >> " << peek_ptr[2]);

  // Place the bytes into the store
  boost::shared_ptr <Frame> list_frame = (memory_ptrs_[channel])->add_block(pkt_size, data_ptr);

  if (list_frame){
    LOG4CXX_INFO(logger_, "Completed frame for channel " << channel << ", pushing");
    // There is a full frame available for pushing
    this->push(list_frame);
  }


/*
  // Check the frame number
  uint32_t frame_id = header->frame_number;

  if (frame_id == 0){
    LOG4CXX_INFO(logger_, "First frame received");
    LOG4CXX_INFO(logger_, "  First channel index: " << header->first_channel);
    LOG4CXX_INFO(logger_, "  Number of channels: " << header->num_channels);
    LOG4CXX_INFO(logger_, "  Number of scalars: " << header->num_scalars);
    LOG4CXX_INFO(logger_, "  Number of resgrades: " << header->num_aux);
  }

  // Check the number of channels.  If the number of channels is different
  // to our previously stored number we must reallocate the memory block
  if (header->num_channels != num_channels_){
    set_number_of_channels(header->num_channels);
  }
  
  // If the frame number is greater than the current memory allocation clear out the memory
  // and update the starting block

  // Split out the frame data into channels and update the memory blocks.
  // If any memory blocks are full then create the frame object and push (this should happen
  // to all of the memory blocks at once).
  uint32_t mca_size = header->num_energy_bins * header->num_aux * sizeof(uint32_t);
  uint32_t num_scalar_values = header->num_scalars * header->num_channels;
  uint32_t num_dtc_factors = header->num_channels;
  uint32_t num_inp_est = header->num_channels;
  uint32_t first_channel_index = header->first_channel;

  char *raw_sca_ptr = frame_bytes;
  raw_sca_ptr += sizeof(ListFrameHeader); 
  uint32_t *sca_ptr = (uint32_t *)raw_sca_ptr;
//  for (int cindex = 0; cindex < num_channels_; cindex++){
//    LOG4CXX_INFO(logger_, "Scalers " << sca_ptr[0] <<
//                          " " << sca_ptr[1] <<
//                          " " << sca_ptr[2] <<
//                          " " << sca_ptr[3] <<
//                          " " << sca_ptr[4] <<
//                          " " << sca_ptr[5] <<
//                          " " << sca_ptr[6] <<
//                          " " << sca_ptr[7] <<
//                          " " << sca_ptr[8]);
//    sca_ptr += 9;
//  }


  char *mca_ptr = frame_bytes;
  mca_ptr += (sizeof(ListFrameHeader) + 
             (num_scalar_values*sizeof(uint32_t)) + 
             (num_dtc_factors*sizeof(double)) + 
             (num_inp_est*sizeof(double))
             );

  for (int index = 0; index < num_channels_; index++){
    memory_ptrs_[index]->add_frame(frame_id, mca_ptr);
    mca_ptr += mca_size;

    // Check if the buffer is full
    if (memory_ptrs_[index]->check_full()){
//        LOG4CXX_INFO(logger_, "Memory block full");

        // Create the frame and push it
        dimensions_t mca_dims;
//        mca_dims.push_back(frames_per_block_);
//        mca_dims.push_back(header->num_aux);
        mca_dims.push_back(header->num_energy_bins);
        std::stringstream ss;
        ss << "mca_" << index + first_channel_index;
        FrameMetaData mca_metadata((frame_id / frames_per_block_), ss.str(), raw_32bit, "", mca_dims);
        boost::shared_ptr<Frame> mca_frame(new DataBlockFrame(mca_metadata, memory_ptrs_[index]->size()));
        memcpy(mca_frame->get_data_ptr(), memory_ptrs_[index]->get_data_ptr(), memory_ptrs_[index]->size());
        // Set the chunking size
        mca_frame->set_outer_chunk_size(frames_per_block_);
        // Push out the MCA data
        this->push(mca_frame);
        // Reset the memory block
        memory_ptrs_[index]->reset();
    } else {
      // Check if we are writing out the last block which is not full size
      if (frame_id == (num_frames_-1)){
        LOG4CXX_INFO(logger_, "num_frames_ - 1: " << (num_frames_-1));
        dimensions_t mca_dims;
        mca_dims.push_back(header->num_energy_bins);
        std::stringstream ss;
        ss << "mca_" << index + first_channel_index;
        FrameMetaData mca_metadata((frame_id / frames_per_block_), ss.str(), raw_32bit, "", mca_dims);
        boost::shared_ptr<Frame> mca_frame(new DataBlockFrame(mca_metadata, memory_ptrs_[index]->current_byte_size()));
        memcpy(mca_frame->get_data_ptr(), memory_ptrs_[index]->get_data_ptr(), memory_ptrs_[index]->current_byte_size());
        // Set the chunking size
        mca_frame->set_outer_chunk_size((int)memory_ptrs_[index]->frames());
        // Push out the MCA data
        this->push(mca_frame);
        // Reset the memory block
        memory_ptrs_[index]->reset();
        LOG4CXX_INFO(logger_, "Pushed partially full frame as required frame count reached");
      }
    }
  }
  */
}

/*
        size_t mcaDataSize = header_->numEnergy * header_->numAux * header_->numChannels * sizeof(uint32_t);
        uint32_t *iSCA = reinterpret_cast<uint32_t *>(frameBytes + sizeof(FrameHeader) + mcaDataSize);

        double clockPeriod = header_->clockPeriod;
        double deadtimeEnergy = header_->deadtimeEnergy;

        dimensions_t mca_dims;
        // set things from data frame
        mca_dims.push_back(header_->numChannels);
        mca_dims.push_back(header_->numAux);
        mca_dims.push_back(header_->numEnergy);
        frame->meta_data().set_dimensions(mca_dims);
        frame->meta_data().set_data_type(XSPRESS_DATA_TYPE);
        frame->meta_data().set_dataset_name("mca");
        frame->set_image_offset(sizeof(FrameHeader));
        frame->set_image_size(mcaDataSize);
        frame->set_frame_number(header_->frameNumber);
        
        frame->meta_data().set_acquisition_ID("");

        FrameProcessor::DataType scaDataType = this->dtcEnabled ? raw_64bit : XSPRESS_DATA_TYPE;

        dimensions_t scaler_dims;
        scaler_dims.push_back(header_->numChannels);
        scaler_dims.push_back(XSP3_SW_NUM_SCALERS);
        FrameMetaData scaler_metadata(header_->frameNumber, "sca", scaDataType, "", scaler_dims);
        // TODO: FIX, how can we allocate or request new blocks?	
        size_t scalerSize = XSP3_SW_NUM_SCALERS*header_->numChannels * (this->dtcEnabled ? sizeof(double) : sizeof(uint32_t));
        boost::shared_ptr <Frame> scaler_frame(new DataBlockFrame(scaler_metadata, scalerSize));

        if (this->dtcEnabled)
        {
            auto dtcFactors = new double[header_->numChannels];
            auto corrected = new double[header_->numChannels];
            calculateDeadtimeCorrection(header_->numChannels, clockPeriod, deadtimeEnergy, iSCA, dtcFactors, corrected);
            deadtimeCorrectScalers(header_->numChannels, iSCA, static_cast<double *>(scaler_frame->get_data_ptr()), dtcFactors, corrected, clockPeriod);
        }
        else
        {
            memcpy(scaler_frame->get_data_ptr(), iSCA, scalerSize);
        }
        this->push(frame);
        this->push(scaler_frame);
        LOG4CXX_TRACE(logger_, "Got frame number: " << header_->frameNumber);
    }
*/


}
