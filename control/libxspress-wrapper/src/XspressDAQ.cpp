/*
 * XspressDetector.cpp
 *
 *  Created on: 24 Sep 2021
 *      Author: Diamond Light Source
 */

#include <stdio.h>

#include "XspressDAQ.h"
#include "DebugLevelLogger.h"

#define BASE_PORT 15150
#define HEADER_ITEMS 5

void free_frame(void *data, void *hint)
{
  free(data);
}

namespace Xspress
{
/** Construct a new XspressDAQ class.
 *
 * The constructor sets up logging used within the class, and initialises
 * variables and threads for processing data.
 */
XspressDAQ::XspressDAQ(LibXspressWrapper *detector_ptr, 
                       uint32_t num_channels,
                       uint32_t num_threads,
                       uint32_t num_spectra):
    logger_(log4cxx::Logger::getLogger("Xspress.XspressDAQ"))
{
  OdinData::configure_logging_mdc(OdinData::app_path.c_str());
  LOG4CXX_DEBUG_LEVEL(1, logger_, "Constructing XspressDAQ");
  detector_ = detector_ptr;
  num_channels_ = num_channels;
  num_threads_ = num_threads;
  num_spectra_ = num_spectra;

  // Create the control thread queue
  ctrl_queue_ = boost::shared_ptr<WorkQueue<boost::shared_ptr<XspressDAQTask> > >(new WorkQueue<boost::shared_ptr<XspressDAQTask> >());
  // Create the control thread
  ctrl_thread_ = new boost::thread(&XspressDAQ::controlTask, this);
  // Create the worker done queue
  done_queue_ = boost::shared_ptr<WorkQueue<boost::shared_ptr<XspressDAQTask> > >(new WorkQueue<boost::shared_ptr<XspressDAQTask> >());

  // Create the ZMQ context
  context_ = new zmq::context_t(num_threads_);

  // Work out how many channels in each thread
  int ch = 0;
  int th = 0;
  std::vector<int> channels;
  channels.resize(num_threads_);
  while (ch < num_channels_){
    channels[th] += 1;
    th++;
    if (th == num_threads_){
        th = 0;
    }
    ch++;
  }
  int cur_chan = 0;
  for (int index = 0; index < num_threads_; ++index){
    LOG4CXX_INFO(logger_, "Creating thread " << index << " for channels " << cur_chan << "-" << cur_chan + channels[index]-1);

    // Create the worker queue for the worker thread
    boost::shared_ptr<WorkQueue<boost::shared_ptr<XspressDAQTask> > > queue;
    queue = boost::shared_ptr<WorkQueue<boost::shared_ptr<XspressDAQTask> > >(new WorkQueue<boost::shared_ptr<XspressDAQTask> >());
    work_queues_.push_back(queue);

    // Create the worker thread
    work_threads_.push_back(new boost::thread(&XspressDAQ::workTask, this, queue, index, cur_chan, channels[index]));
    cur_chan += channels[index];

  }
}

/** Destructor for XspressDetector class.
 *
 */
XspressDAQ::~XspressDAQ()
{

}

void XspressDAQ::set_num_aux_data(uint32_t num_aux_data)
{
    num_aux_data_ = num_aux_data;
}

boost::shared_ptr<XspressDAQTask> XspressDAQ::create_task(uint32_t type)
{
  return create_task(type, 0);
}

boost::shared_ptr<XspressDAQTask> XspressDAQ::create_task(uint32_t type, uint32_t value1)
{
  return create_task(type, value1, 0);
}

boost::shared_ptr<XspressDAQTask> XspressDAQ::create_task(uint32_t type, uint32_t value1, uint32_t value2)
{
  boost::shared_ptr<XspressDAQTask> task = boost::shared_ptr<XspressDAQTask>(new XspressDAQTask());
  task->type_ = type;
  task->value1_ = value1;
  task->value2_ = value2;
  return task;
}

void XspressDAQ::startAcquisition(uint32_t frames)
{
  // Load the start task into the ctrl queue
  ctrl_queue_->add(create_task(DAQ_TASK_TYPE_START, frames), true);
}

void XspressDAQ::controlTask()
{
  LOG4CXX_INFO(logger_, "Starting control task with ID [" << boost::this_thread::get_id() << "]");
  bool executing = true;
  while (executing){
    boost::shared_ptr<XspressDAQTask> task = ctrl_queue_->remove();
    if (task->type_ == DAQ_TASK_TYPE_START){
      int32_t total_frames = task->value1_;
      LOG4CXX_INFO(logger_, "DAQ ctrl thread started with [" << total_frames << "] frames");

      // Validate the histogram dimensions
      detector_->validate_histogram_dims(num_spectra_,
                                         num_aux_data_,
                                         0,
                                         num_channels_,
                                         &buffer_length_);

      LOG4CXX_INFO(logger_, "Buffer length calculated: [" << buffer_length_ << "]");


      int32_t num_frames = 0;
      int32_t frames_read = 0;
      bool acq_running = true;
      while ((num_frames < total_frames) && acq_running){
        int status = detector_->get_num_frames_read(&num_frames);
        if (status == XSP_STATUS_OK){
          uint32_t frames_to_read = num_frames - frames_read;
          if (frames_to_read > 0){
              LOG4CXX_DEBUG_LEVEL(0, logger_, "Current frames to read: " << frames_read << " - " << num_frames-1);
            // Notify the worker threads to process the frames
            std::vector<boost::shared_ptr<WorkQueue<boost::shared_ptr<XspressDAQTask> > > >::iterator iter;
            for (iter = work_queues_.begin(); iter != work_queues_.end(); ++iter){
              (*iter)->add(create_task(DAQ_TASK_TYPE_READ, frames_read, frames_to_read));
            }
            // Now wait for the worker threads to complete their processing
            LOG4CXX_DEBUG_LEVEL(4, logger_, "Waiting for " << num_threads_ << " worker threads to complete");
            for (int index = 0; index < num_threads_; index++){
              done_queue_->remove();
            }
            // Once all worker threads have completed notify the circular buffer
            status = detector_->histogram_circ_ack(0, frames_read, frames_to_read, num_channels_);
            LOG4CXX_DEBUG_LEVEL(0, logger_, "Ack circular buffer [status=" << status << "] frames_read[" << frames_read << "] frames_to_read[" << frames_to_read << "]");
            LOG4CXX_DEBUG_LEVEL(4, logger_, "Worker threads completed and circular buffer acknowledgement sent");
            // Set frames read to correct value
            frames_read = num_frames;
          } else {
              // We need to wait for some frames
              sleep(0.001);
          }
        } else {
          LOG4CXX_ERROR(logger_, "Error: " << "INSERT MSG" << " Aborting acquisition");
          acq_running = false;
          // Abort the acquisition
          detector_->histogram_stop(0);
          //detector_->histogram_stop(1);
          //detector_->histogram_stop(2);
          //detector_->histogram_stop(3);
        }
      }
      LOG4CXX_INFO(logger_, "DAQ thread completed, read " << frames_read << " frames");
    }
  }
}

void XspressDAQ::workTask(boost::shared_ptr<WorkQueue<boost::shared_ptr<XspressDAQTask> > > queue,
                          int index,
                          int channel_index,
                          int num_channels)
{
  int status = XSP_STATUS_OK;
  uint32_t num_scalars = 0;

  LOG4CXX_INFO(logger_, "Starting work task with ID [" << boost::this_thread::get_id() << "]");

  detector_->get_num_scalars(&num_scalars);

  // Create the ZMQ endpoint for this worker
  std::stringstream ss;
  ss << "tcp://127.0.0.1:" << (index + BASE_PORT);
  LOG4CXX_INFO(logger_, "workTask[" << index << "] => Creating zmq socket and binding to [" << ss.str() << "]");
  zmq::socket_t *data_socket = new zmq::socket_t(*context_, ZMQ_PUSH);
  data_socket->bind(ss.str().c_str());

  bool executing = true;
  while (executing){
    boost::shared_ptr<XspressDAQTask> task = queue->remove();

    if (task->type_ == DAQ_TASK_TYPE_READ){
      int32_t frames_read = task->value1_;
      int32_t frames_to_read = task->value2_;
      if (frames_to_read > 0){
        LOG4CXX_DEBUG_LEVEL(4, logger_, "workTask[" << index << "] => reading frames [" << frames_to_read << "]"); 

        uint32_t header_size = HEADER_ITEMS * sizeof(uint32_t);
        uint32_t data_size = num_spectra_ * num_channels * num_aux_data_ * sizeof(uint32_t);
        uint32_t scalar_size = num_channels * num_scalars * sizeof(uint32_t);
        uint32_t dtc_size = num_channels * sizeof(double);
        uint32_t inp_est_size = num_channels * sizeof(double);
        uint32_t frame_size = header_size + data_size + scalar_size + dtc_size + inp_est_size;

        LOG4CXX_DEBUG_LEVEL(4, logger_, "workTask[" << index << "] => Num scalars: [" << num_scalars << "] scalar_size: [" << scalar_size << "]");
        LOG4CXX_DEBUG_LEVEL(4, logger_, "workTask[" << index << "] => Calculated frame size: [" << frame_size << "]");

        //LOG4CXX_INFO(logger_, "Calling memcpy at channel: " << channel_index << " for " << num_channels << " channels");

        // We need to avoid memcpy as much as possible, and we also need to allow ZMQ to free the memory
        // when it is read to do so.  Therefore we will perform a single frame allocation and 1 memcpy,
        // and then we can safely let ZMQ free that memory block at any time.

        for (int current_frame = 0; current_frame < frames_to_read; current_frame++){
          // Allocation of memory:
          // 1 x uint32 => Frame number
          // 1 x uint32 => Num spectra
          // 1 x uint32 => Num aux data
          // 1 x uint32 => Num channels
          // Frame data [num_spectra x num_channels x num_aux_data x uint32]
          unsigned char *base_ptr;
          uint32_t *frame_ptr = (uint32_t *)malloc(frame_size);
          uint32_t *h_ptr = frame_ptr;
          base_ptr = (unsigned char *)frame_ptr;
          base_ptr += header_size;
          uint32_t *s_ptr = (uint32_t *)base_ptr;
          base_ptr = (unsigned char *)frame_ptr;
          base_ptr += (header_size + scalar_size);
          double *dtc_ptr = (double *)base_ptr;
          base_ptr = (unsigned char *)frame_ptr;
          base_ptr += (header_size + scalar_size + dtc_size);
          double *inp_est_ptr = (double *)base_ptr;
          base_ptr = (unsigned char *)frame_ptr;
          base_ptr += (header_size + scalar_size + dtc_size + inp_est_size);
          uint32_t *d_ptr = (uint32_t *)base_ptr;

//        LOG4CXX_DEBUG_LEVEL(4, logger_, "workTask[" << index << "] => frame_ptr: [" << frame_ptr << "]");
//        LOG4CXX_DEBUG_LEVEL(4, logger_, "workTask[" << index << "] => h_ptr: [" << h_ptr << "]");
//        LOG4CXX_DEBUG_LEVEL(4, logger_, "workTask[" << index << "] => s_ptr: [" << s_ptr << "]");
//        LOG4CXX_DEBUG_LEVEL(4, logger_, "workTask[" << index << "] => d_ptr: [" << d_ptr << "]");


          // Fill in the header data items
          h_ptr[0] = frames_read + current_frame;
          h_ptr[1] = num_spectra_;
          h_ptr[2] = num_aux_data_;
          h_ptr[3] = num_channels;
          h_ptr[4] = num_scalars;

          // Perform the single frame memcpy
          status = detector_->histogram_memcpy(d_ptr,
                                               frames_read + current_frame,
                                               1, // Read a single frame
                                               buffer_length_,
                                               num_spectra_,
                                               num_aux_data_,
                                               channel_index,
                                               num_channels);

          // Perform the scalar memcpy
          status = detector_->scaler_read(s_ptr,
                                          frames_read + current_frame,
                                          1, // Read a single frame
                                          channel_index,
                                          num_channels);


          // Calculate the Dead Time Correction factors
          status = detector_->calculate_dtc_factors(s_ptr,
                                                    dtc_ptr,
                                                    inp_est_ptr,
                                                    1,
                                                    channel_index,
                                                    num_channels);

//        for (int cindex = 0; cindex < num_channels; cindex++){
//            int total = 0;
//            for (int sindex = 0; sindex < num_spectra_; sindex++){
//            total += pMCAData[sindex + (cindex*num_spectra_)];
//            }
//            //LOG4CXX_DEBUG_LEVEL(1, logger_, "MCA channel[" << channel_index+cindex << "] total: " << total);
//        }

          LOG4CXX_DEBUG_LEVEL(4, logger_, "workTask[" << index << "] => sending ZMQ message");
          // Construct the ZMQ message wrapper and send the frame
          zmq::message_t frame_data(frame_ptr, frame_size, free_frame);
          data_socket->send(frame_data, 0);
          LOG4CXX_DEBUG_LEVEL(4, logger_, "workTask[" << index << "] => message sent");
        }
      }
      // Notify we have completed the task
      done_queue_->add(create_task(DAQ_TASK_TYPE_COMPLETE), true);
      LOG4CXX_DEBUG_LEVEL(4, logger_, "workTask[" << index << "] => done_queue notification complete");
    }
  }
}

//                TODO: this metadata stuff is probably quite fragile...make it better
//                (sensitive to sizes of data types...should always be 64 bits for each of the types but maybe not?)
//
//                zmq::message_t frameMeta(4*sizeof(u_int64_t) + 2*sizeof(double));
//                memcpy(frameMeta.data(), dims, 3 * sizeof(size_t));
//                static_cast<u_int64_t *>(frameMeta.data())[3] = thisNDArrayCounter;
//                static_cast<double *>(frameMeta.data())[4] = Xsp3Sys[xsp3_handle_].clock_period;
//                static_cast<double *>(frameMeta.data())[5] = Xsp3Sys[xsp3_handle_].deadtimeEnergy;
//
//                int receiver = i % this->numReceivers_;
//                zmq::message_t frameData(MCABuffer[i], frame_size, free_multi);
//                socket_[receiver]->send(frameData, ZMQ_SNDMORE);
//                zmq::message_t intMetaData(iSCA, intMeta_size, free_multi);
//                if (this->dtcParamsUpdated) {
//                    socket_[receiver]->send(intMetaData, ZMQ_SNDMORE);
//                    zmq::message_t dblMetaData(dblMeta_size);
//                    memcpy(dblMetaData.data(), dDTC, dblMeta_size);
//                    socket_[receiver]->send(dblMetaData, 0);
//                } else {
//                    socket_[receiver]->send(intMetaData, 0);
//                }

} /* namespace Xspress */
