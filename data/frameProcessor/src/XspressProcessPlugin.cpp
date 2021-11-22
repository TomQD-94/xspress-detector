//
// Created by hir12111 on 03/11/18.
//
#include <iostream>
#include "DataBlockFrame.h"
#include "XspressProcessPlugin.h"
#include "FrameProcessorDefinitions.h"
#include "XspressDefinitions.h"

namespace FrameProcessor {

const std::string XspressProcessPlugin::CONFIG_DTC_FLAGS = "dtc/flags";
const std::string XspressProcessPlugin::CONFIG_DTC_PARAMS = "dtc/params";


XspressMemoryBlock::XspressMemoryBlock() :
  ptr_(0),
  num_bytes_(0),
  frames_(0),
  max_frames_(0),
  frame_size_(0)
{
  // Setup logging for the class
  logger_ = Logger::getLogger("FP.XspressProcessPlugin");
  LOG4CXX_INFO(logger_, "Created XspressMemoryBlock");
}

XspressMemoryBlock::~XspressMemoryBlock()
{
  if (ptr_){
    free(ptr_);
  }
}

void XspressMemoryBlock::set_size(uint32_t frame_size, uint32_t max_frames)
{
  frame_size_ = frame_size;
  max_frames_ = max_frames;
  num_bytes_ = frame_size * max_frames;
  reallocate();
}

void XspressMemoryBlock::reallocate()
{
  LOG4CXX_INFO(logger_, "Reallocating XspressMemoryBlock to [" << num_bytes_ << "] bytes");
  if (ptr_){
    free(ptr_);
  }
  ptr_ = (char *)malloc(num_bytes_);
  reset();
}

void XspressMemoryBlock::reset()
{
  memset(ptr_, 0, num_bytes_);
  frames_ = 0;
}

void XspressMemoryBlock::add_frame(uint32_t frame_id, char *ptr)
{
//  LOG4CXX_INFO(logger_, "Adding frame [" << frame_id << "] to XspressMemoryBlock");
  // Work out the pointer offset
  uint32_t frame_offset = frame_id % max_frames_;
  char *dest = ptr_;
  dest += (frame_offset * frame_size_);
  memcpy(dest, ptr, frame_size_);
  frames_ += 1;
//  LOG4CXX_INFO(logger_, "Frames [" << frames_ << " / " << max_frames_ << "]");
}

bool XspressMemoryBlock::check_full()
{
  bool full = false;
//  LOG4CXX_INFO(logger_, "Check full [" << frames_ << " / " << max_frames_ << "]");
  if (frames_ == max_frames_){
    full = true;
  }
  return full;
}

uint32_t XspressMemoryBlock::size()
{
  return num_bytes_;
}

char *XspressMemoryBlock::get_data_ptr()
{
  return ptr_;
}


XspressProcessPlugin::XspressProcessPlugin() :
  num_energy_bins_(4096),
  num_channels_(0),
  frames_per_block_(256),
  current_block_start_(0)
{
  // Setup logging for the class
  logger_ = Logger::getLogger("FP.XspressProcessPlugin");
  LOG4CXX_TRACE(logger_, "XspressProcessPlugin constructor.");
}

XspressProcessPlugin::~XspressProcessPlugin()
{
  LOG4CXX_TRACE(logger_, "XspressProcessPlugin destructor.");
}

void XspressProcessPlugin::configure(OdinData::IpcMessage& config, OdinData::IpcMessage& reply) 
{
/*         if (dtcParams != NULL) delete [] dtcParams;
         if (dtcFlags != NULL) delete [] dtcFlags;
         const rapidjson::Value& params = config.get_param<const rapidjson::Value&>(CONFIG_DTC_PARAMS);
         const rapidjson::Value& flags = config.get_param<const rapidjson::Value&>(CONFIG_DTC_FLAGS);
         this->dtcParams = new double[XSP3_NUM_DTC_FLOAT_PARAMS * params.Size()];
         this->dtcFlags = new int[flags.Size()];
         if (flags.Size() != params.Size()) {
             LOG4CXX_ERROR(logger_, "DTC setting dimension mismatch");
             return;
         }

         size_t numChannels = flags.Size();
         for (size_t i = 0; i < numChannels; i++) {
             const rapidjson::Value& flag = flags[i];
             const rapidjson::Value& chanParams = params[i];
             this->dtcFlags[i] = flag.GetUint64();
             if (chanParams.Size() != XSP3_NUM_DTC_FLOAT_PARAMS) {
                 LOG4CXX_ERROR(logger_, "DTC setting dimension mismatch");
                 return;
             }
             for (rapidjson::SizeType j = 0; j < XSP3_NUM_DTC_FLOAT_PARAMS; j++) {
                 const rapidjson::Value& param = chanParams[j];
                 this->dtcParams[i * XSP3_NUM_DTC_FLOAT_PARAMS + j] = param.GetDouble();
             }
         }
*/
}

// Version functions
int XspressProcessPlugin::get_version_major() 
{
  return XSPRESS_DETECTOR_VERSION_MAJOR;
}

int XspressProcessPlugin::get_version_minor()
{
  return XSPRESS_DETECTOR_VERSION_MINOR;
}

int XspressProcessPlugin::get_version_patch()
{
  return XSPRESS_DETECTOR_VERSION_PATCH;
}

std::string XspressProcessPlugin::get_version_short()
{
  return XSPRESS_DETECTOR_VERSION_STR_SHORT;
}

std::string XspressProcessPlugin::get_version_long()
{
  return XSPRESS_DETECTOR_VERSION_STR;
}

void XspressProcessPlugin::set_number_of_channels(uint32_t num_channels)
{
  num_channels_ = num_channels;
  // We must reallocate memory blocks
  setup_memory_allocation();
}

void XspressProcessPlugin::set_number_of_aux(uint32_t num_aux)
{
  num_aux_ = num_aux;
  // We must reallocate memory blocks
  setup_memory_allocation();
}

void XspressProcessPlugin::setup_memory_allocation()
{
  // First clear out the memory vector emptying any blocks
  memory_ptrs_.clear();

  // Allocate large enough blocks of memory to hold frames_per_block spectra
  // Allocate one block of memory for each channel
  uint32_t frame_size = num_energy_bins_ * num_aux_ * sizeof(uint32_t);
  for (int index = 0; index <= num_channels_; index++){
    boost::shared_ptr<XspressMemoryBlock> ptr = boost::shared_ptr<XspressMemoryBlock>(new XspressMemoryBlock());
    ptr->set_size(frame_size, frames_per_block_);
    memory_ptrs_.push_back(ptr);
  }
}

void XspressProcessPlugin::process_frame(boost::shared_ptr <Frame> frame) 
{
  char* frame_bytes = static_cast<char *>(frame->get_data_ptr());	
  FrameHeader *header = reinterpret_cast<FrameHeader *>(frame_bytes);

  // Check the frame number
  uint32_t frame_id = header->frame_number;

  if (frame_id == 0){
    LOG4CXX_INFO(logger_, "First frame received");
  }

  // Check the number of channels.  If the number of channels is different
  // to our previously stored number we must reallocate the memory block
  if (header->num_channels != num_channels_){
    set_number_of_channels(header->num_channels);
  }

  // Check the number of aux values.  If the number of is different
  // to our previously stored number we must reallocate the memory block
  if (header->num_aux != num_aux_){
    set_number_of_aux(header->num_aux);
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

  char *mca_ptr = frame_bytes;
  mca_ptr += (sizeof(FrameHeader) + 
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
        mca_dims.push_back(frames_per_block_);
//        mca_dims.push_back(header->num_aux);
        mca_dims.push_back(header->num_energy_bins);
        std::stringstream ss;
        ss << "mca_" << index;
        FrameMetaData mca_metadata((frame_id / frames_per_block_), ss.str(), raw_32bit, "", mca_dims);
        boost::shared_ptr<Frame> mca_frame(new DataBlockFrame(mca_metadata, memory_ptrs_[index]->size()));
        memcpy(mca_frame->get_data_ptr(), memory_ptrs_[index]->get_data_ptr(), memory_ptrs_[index]->size());
        // Push out the MCA data
        this->push(mca_frame);
        // Reset the memory block
        memory_ptrs_[index]->reset();
    } else {
//        LOG4CXX_INFO(logger_, "Memory block not full");
    }
  }
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
