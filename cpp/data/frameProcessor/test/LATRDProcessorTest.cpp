/*
 * FileWriterTest.cpp
 *
 */
#define BOOST_TEST_MODULE "FrameProcessorUnitTests"
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include <log4cxx/logger.h>
#include <log4cxx/xml/domconfigurator.h>
#include <log4cxx/simplelayout.h>
#include <log4cxx/consoleappender.h>
#include <log4cxx/basicconfigurator.h>
using namespace log4cxx;
using namespace log4cxx::xml;

#include "LATRDBuffer.h"
#include "LATRDProcessCoordinator.h"
#include "LATRDTimestampManager.h"
#include "FrameMetaData.h"
#include "DataBlockFrame.h"

class GlobalConfig {
public:
  GlobalConfig() {
    //std::cout << "GlobalConfig constructor" << std::endl;
    // Create a default simple console appender for log4cxx.
    consoleAppender = new ConsoleAppender(LayoutPtr(new SimpleLayout()));
    BasicConfigurator::configure(AppenderPtr(consoleAppender));
    Logger::getRootLogger()->setLevel(Level::getWarn());
  }
  ~GlobalConfig() {
    //std::cout << "GlobalConfig constructor" << std::endl;
    //delete consoleAppender;
  };
private:
  ConsoleAppender *consoleAppender;
};
BOOST_GLOBAL_FIXTURE(GlobalConfig);

BOOST_AUTO_TEST_SUITE(BufferUnitTest);

BOOST_AUTO_TEST_CASE(BufferTest)
{
  // Create a buffer with UINT32 type
  FrameProcessor::LATRDBuffer buffer(20, "test_buffer", FrameProcessor::UINT32_TYPE);
  uint64_t data1[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  boost::shared_ptr<FrameProcessor::Frame> frame;
  // Append 10 points, verify no frame is returned
  BOOST_CHECK_NO_THROW(frame = buffer.appendData(data1, 10));
  BOOST_CHECK(!frame);
  // Append another 10 points, verify a frame is returned
  BOOST_CHECK_NO_THROW(frame = buffer.appendData(data1, 10));
  BOOST_CHECK(frame);
  // Verify the frame has frame number 0
  BOOST_CHECK_EQUAL(frame->get_frame_number(), 0);
  // Append 10 points, verify no frame is returned
  BOOST_CHECK_NO_THROW(frame = buffer.appendData(data1, 10));
  BOOST_CHECK(!frame);
  // Append another 10 points, verify a frame is returned
  BOOST_CHECK_NO_THROW(frame = buffer.appendData(data1, 10));
  BOOST_CHECK(frame);
  // Verify the frame has frame number 1
  BOOST_CHECK_EQUAL(frame->get_frame_number(), 1);

  // Set the rank and process number
  buffer.configureProcess(3, 1);
  // Append 10 points, verify no frame is returned
  BOOST_CHECK_NO_THROW(frame = buffer.appendData(data1, 10));
  BOOST_CHECK(!frame);
  // Append another 10 points, verify a frame is returned
  BOOST_CHECK_NO_THROW(frame = buffer.appendData(data1, 10));
  BOOST_CHECK(frame);
  // Verify the frame has frame number 7
  BOOST_CHECK_EQUAL(frame->get_frame_number(), 7);

}

BOOST_AUTO_TEST_SUITE_END(); //BufferUnitTest


BOOST_AUTO_TEST_SUITE(CoordinatorUnitTest);

BOOST_AUTO_TEST_CASE(CoordinatorTest)
{
  // Create a coordinator class with 8 time slice buffers
  FrameProcessor::LATRDProcessCoordinator cd();

  // Create some frames with appropriate IDs
  unsigned long long ids[26] = {1, 2, 3, 4, 5,
                                1, 2, 3,
                                1, 2, 3,
                                1, 2, 3,
                                1, 2, 3,
                                1, 2, 3,
                                1, 2, 3,
                                1, 2, 3
  };

  boost::shared_ptr<FrameProcessor::Frame> frames[26];
  for (int index = 0; index < 26; index++){
 		FrameProcessor::FrameMetaData frame_meta;
    std::vector<dimsize_t> dims(0);
    frame_meta.set_dataset_name("test");
    frame_meta.set_data_type(FrameProcessor::raw_16bit);
    frame_meta.set_dimensions(dims);
    frame_meta.set_compression_type(FrameProcessor::no_compression);
		frames[index] = boost::shared_ptr<FrameProcessor::DataBlockFrame>(new FrameProcessor::DataBlockFrame(frame_meta, 10));
//    frames[index] = boost::shared_ptr<FrameProcessor::Frame>(new FrameProcessor::Frame("test"));
    frames[index]->set_frame_number(ids[index]);
  }

  // Add frames 1, 2, 4, 5 with wrap 0, buffer 0
//  cd.add_frame(0, 0, frames[0]);
//  cd.add_frame(0, 0, frames[1]);
//  cd.add_frame(0, 0, frames[3]);
//  cd.add_frame(0, 0, frames[4]);

  // Now request frames 1 to 5, should get back NULL for 3
  boost::shared_ptr<FrameProcessor::Frame> fptr;

/*  BOOST_CHECK_NO_THROW(fptr = cd.get_frame(0, 0, 1));
  BOOST_CHECK_EQUAL(fptr->get_frame_number(), 1);
  BOOST_CHECK_NO_THROW(fptr = cd.get_frame(0, 0, 2));
  BOOST_CHECK_EQUAL(fptr->get_frame_number(), 2);
  BOOST_CHECK_NO_THROW(fptr = cd.get_frame(0, 0, 3));
  BOOST_CHECK(!fptr);
  BOOST_CHECK_NO_THROW(fptr = cd.get_frame(0, 0, 4));
  BOOST_CHECK_EQUAL(fptr->get_frame_number(), 4);
  BOOST_CHECK_NO_THROW(fptr = cd.get_frame(0, 0, 5));
  BOOST_CHECK_EQUAL(fptr->get_frame_number(), 5);

  // Now request frames 1 and 2 again, and check we get back NULL
  BOOST_CHECK_NO_THROW(fptr = cd.get_frame(0, 0, 1));
  BOOST_CHECK(!fptr);
  BOOST_CHECK_NO_THROW(fptr = cd.get_frame(0, 0, 2));
  BOOST_CHECK(!fptr);

  // Add frames 1, 2, 3 with wrap 0, buffer 0
  cd.add_frame(0, 0, frames[0]);
  cd.add_frame(0, 0, frames[1]);
  cd.add_frame(0, 0, frames[2]);

  // Add frame 1 with wrap 0, buffer 1 and verify we get an empty array back
  std::vector<boost::shared_ptr<FrameProcessor::Frame> > vec;
  BOOST_CHECK_NO_THROW(vec = cd.add_frame(0, 1, frames[0]));
  BOOST_CHECK_EQUAL(vec.size(), 0);

  // Add frame 1 with wrap 0, buffer 2 and verify we get an empty array back
  BOOST_CHECK_NO_THROW(vec = cd.add_frame(0, 2, frames[0]));
  BOOST_CHECK_EQUAL(vec.size(), 0);

  // Add frame 1 with wrap 0, buffer 3 and verify we get an empty array back
  BOOST_CHECK_NO_THROW(vec = cd.add_frame(0, 3, frames[0]));
  BOOST_CHECK_EQUAL(vec.size(), 0);

  // Add frame 1 with wrap 0, buffer 4 and verify we get an empty array back
  BOOST_CHECK_NO_THROW(vec = cd.add_frame(0, 4, frames[0]));
  BOOST_CHECK_EQUAL(vec.size(), 0);

  // Add frame 1 with wrap 1 buffer 0 and verify that we get back an array of size 3
  BOOST_CHECK_NO_THROW(vec = cd.add_frame(1, 0, frames[0]));
  BOOST_CHECK_EQUAL(vec.size(), 3);
  // Verify that the vector contains frames 1, 2, 3
  BOOST_CHECK_EQUAL(vec[0]->get_frame_number(), 1);
  BOOST_CHECK_EQUAL(vec[1]->get_frame_number(), 2);
  BOOST_CHECK_EQUAL(vec[2]->get_frame_number(), 3);

  // Add frame 2 with wrap 0, buffer 1 and verify we get an empty array back
  BOOST_CHECK_NO_THROW(vec = cd.add_frame(0, 1, frames[1]));
  BOOST_CHECK_EQUAL(vec.size(), 0);
  // Add frame 4 with wrap 0, buffer 1 and verify we get an empty array back
  BOOST_CHECK_NO_THROW(vec = cd.add_frame(0, 1, frames[3]));
  BOOST_CHECK_EQUAL(vec.size(), 0);

  // Request to clear wrap 0, buffer 1 and verify we get back an array of size 3
  BOOST_CHECK_NO_THROW(vec = cd.clear_buffer(0, 1));
  BOOST_CHECK_EQUAL(vec.size(), 3);
  // Verify that the vector contains frames 1, 2, 4
  BOOST_CHECK_EQUAL(vec[0]->get_frame_number(), 1);
  BOOST_CHECK_EQUAL(vec[1]->get_frame_number(), 2);
  BOOST_CHECK_EQUAL(vec[2]->get_frame_number(), 4);
*/
}

BOOST_AUTO_TEST_SUITE_END(); //CoordinatorUnitTest

BOOST_AUTO_TEST_SUITE(TimeSliceWrapUnitTest);

BOOST_AUTO_TEST_CASE(TimeSliceWrapTest)
{
  // Create a test process job
  boost::shared_ptr<FrameProcessor::LATRDProcessJob> job;
  job = boost::shared_ptr<FrameProcessor::LATRDProcessJob>(new FrameProcessor::LATRDProcessJob(1));
  // Create a time slice wrap with 8 buffers
  FrameProcessor::LATRDTimeSliceWrap wrap(8);

  // Add a process job with an incorrect wrap
  wrap.add_job(10, job);

  // Now empty out the buffers and verify there were none saved into the wrap
  std::vector<boost::shared_ptr<FrameProcessor::LATRDProcessJob> > jobs = wrap.empty_all_buffers();
  BOOST_CHECK_EQUAL(jobs.size(), 0);

  // Now add jobs into valid buffers
  job->packet_number = 0;
  wrap.add_job(1, job);
  job->packet_number = 1;
  wrap.add_job(1, job);
  job->packet_number = 0;
  wrap.add_job(2, job);
  job->packet_number = 1;
  wrap.add_job(2, job);
  job->packet_number = 2;
  wrap.add_job(2, job);
  job->packet_number = 0;
  wrap.add_job(3, job);
  job->packet_number = 1;
  wrap.add_job(3, job);
  job->packet_number = 2;
  wrap.add_job(3, job);
  job->packet_number = 3;
  wrap.add_job(3, job);

  // Check there are 2 jobs in buffer 1
  jobs = wrap.empty_buffer(1);
  BOOST_CHECK_EQUAL(jobs.size(), 2);
  // Check there are 3 jobs in buffer 2
  jobs = wrap.empty_buffer(2);
  BOOST_CHECK_EQUAL(jobs.size(), 3);
  // Check there are 4 jobs in buffer 3
  jobs = wrap.empty_buffer(3);
  BOOST_CHECK_EQUAL(jobs.size(), 4);

  // Now add the jobs back into valid buffers
  job->packet_number = 0;
  wrap.add_job(1, job);
  job->packet_number = 1;
  wrap.add_job(1, job);
  job->packet_number = 0;
  wrap.add_job(2, job);
  job->packet_number = 1;
  wrap.add_job(2, job);
  job->packet_number = 2;
  wrap.add_job(2, job);
  job->packet_number = 0;
  wrap.add_job(3, job);
  job->packet_number = 1;
  wrap.add_job(3, job);
  job->packet_number = 2;
  wrap.add_job(3, job);
  job->packet_number = 3;
  wrap.add_job(3, job);

  // Check there are 9 jobs in total
  jobs = wrap.empty_all_buffers();
  BOOST_CHECK_EQUAL(jobs.size(), 9);

  // Now add the jobs back into valid buffers
  // and add some valid result counters
  job->packet_number = 0;
  job->valid_results = 20;
  wrap.add_job(1, job);
  job->packet_number = 1;
  job->valid_results = 30;
  wrap.add_job(1, job);
  job->packet_number = 0;
  job->valid_results = 40;
  wrap.add_job(2, job);
  job->packet_number = 1;
  job->valid_results = 50;
  wrap.add_job(2, job);
  job->packet_number = 2;
  job->valid_results = 60;
  wrap.add_job(2, job);
  job->packet_number = 0;
  job->valid_results = 70;
  wrap.add_job(3, job);
  job->packet_number = 1;
  job->valid_results = 80;
  wrap.add_job(3, job);
  job->packet_number = 2;
  job->valid_results = 90;
  wrap.add_job(3, job);
  job->packet_number = 3;
  job->valid_results = 100;
  wrap.add_job(3, job);

  // Check that the total event counts for buffer 1 is 50
  BOOST_CHECK_EQUAL(wrap.get_event_data_counts(1), 50);

  // Check that the total event counts for all buffers is 540
  std::vector<uint32_t> events = wrap.get_all_event_data_counts();
  uint32_t total_counts = 0;
  for (std::vector<uint32_t>::iterator iter = events.begin(); iter != events.end(); ++iter){
    total_counts += *iter;
  }
  BOOST_CHECK_EQUAL(total_counts, 540);
}

BOOST_AUTO_TEST_SUITE_END(); //TimeSliceWrapUnitTest

BOOST_AUTO_TEST_SUITE(ProcessJobUnitTest);

BOOST_AUTO_TEST_CASE(ProcessJobTest)
{
  // Create a ProcessJob object
  FrameProcessor::LATRDProcessJob job(20);
  // Set the values of the public member variables
  job.time_slice = 1;
  job.job_id = 2;
  job.invalid = 3;
  job.packet_number = 4;
  job.valid_control_words = 5;
  job.timestamp_mismatches = 6;
  job.words_to_process = 7;
  job.valid_results = 8;

  // Reset the object
  BOOST_CHECK_NO_THROW(job.reset());
  // Verify the values are all set to 0
  BOOST_CHECK_EQUAL(job.time_slice, 0);
  BOOST_CHECK_EQUAL(job.job_id, 0);
  BOOST_CHECK_EQUAL(job.invalid, 0);
  BOOST_CHECK_EQUAL(job.packet_number, 0);
  BOOST_CHECK_EQUAL(job.valid_control_words, 0);
  BOOST_CHECK_EQUAL(job.timestamp_mismatches, 0);
  BOOST_CHECK_EQUAL(job.words_to_process, 0);
  BOOST_CHECK_EQUAL(job.valid_results, 0);

}

BOOST_AUTO_TEST_SUITE_END(); //ProcessJobUnitTest
