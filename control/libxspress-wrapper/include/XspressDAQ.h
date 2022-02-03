/*
 * XspressDAQ.h
 *
 *  Created on: 25 Oct 2021
 *      Author: Diamond Light Source
 */

#ifndef XspressDAQ_H_
#define XspressDAQ_H_

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/helpers/exception.h>

#include "zmq/zmq.hpp"

#include "WorkQueue.h"
#include "logging.h"
#include "LibXspressWrapper.h"

using namespace log4cxx;
using namespace log4cxx::helpers;
using namespace FrameProcessor;

// Define the types of DAQ tasks
#define DAQ_TASK_TYPE_START    0
#define DAQ_TASK_TYPE_READ     1
#define DAQ_TASK_TYPE_COMPLETE 2
#define DAQ_TASK_TYPE_SHUTDOWN 3

namespace Xspress
{

class XspressDAQTask
{
public:
  uint32_t type_;
  uint32_t value1_;
  uint32_t value2_;
};

/**
 * The XspressDAQ class has responsibility for creating threads to process frames
 * as they become available from the libxspress library during an acquisition.  
 * 
 * This class is designed to have no dependency on the physical hardware of the 
 * detector; it can run any number of threads and each thread can monitor any
 * number of channels.  The only limitation is that the number of useful threads 
 * is limited to the number of channels.
 */
class XspressDAQ
{
public:
  XspressDAQ(LibXspressWrapper *detector_ptr,
             uint32_t num_channels,
             uint32_t num_spectra,
             std::vector<std::string> endpoints);
  virtual ~XspressDAQ();
  void set_num_aux_data(uint32_t num_aux_data);
  boost::shared_ptr<XspressDAQTask> create_task(uint32_t type);
  boost::shared_ptr<XspressDAQTask> create_task(uint32_t type, uint32_t value1);
  boost::shared_ptr<XspressDAQTask> create_task(uint32_t type, uint32_t value1, uint32_t value2);
  void startAcquisition(uint32_t frames);
  void controlTask();
  void workTask(boost::shared_ptr<WorkQueue<boost::shared_ptr<XspressDAQTask> > > queue,
                int index,
                int channel_index,
                int num_channels,
                const std::string& endpoint);

private:
  /** libxspress wrapper object ptr */
  LibXspressWrapper             *detector_;
  /** Pointer to the logging facility */
  log4cxx::LoggerPtr            logger_;
  /** Number of channels to monitor */
  uint32_t                      num_channels_;
  /** Number of threads to spawn */
  uint32_t                      num_threads_;
  /** Number of auxiliary data items */
  int                           num_aux_data_;
  /** Number of spectra */
  int                           num_spectra_;
  /** Pointer to worker queue thread */
  boost::thread                 *thread_;
  /** Pointer to control thread */
  boost::thread                 *ctrl_thread_;
  //* Vector of worker thred pointers */
  std::vector<boost::thread *>  work_threads_;
  /** Pointer to control thread task queue */
  boost::shared_ptr<WorkQueue<boost::shared_ptr<XspressDAQTask> > > ctrl_queue_;
  /** Vector of pointers to the worker thread queues */
  std::vector<boost::shared_ptr<WorkQueue<boost::shared_ptr<XspressDAQTask> > > > work_queues_;
  /** Pointer to worker thread completion queue */
  boost::shared_ptr<WorkQueue<boost::shared_ptr<XspressDAQTask> > > done_queue_;
  /** ZeroMQ context */
  zmq::context_t                *context_;

  /** Buffer length value */
  uint32_t                      buffer_length_;

};

} /* namespace Xspress */

#endif /* XspressDAQ_H_ */
