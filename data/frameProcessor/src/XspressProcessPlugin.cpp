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

    static double xsp3_correct_tp_lin(double ffr, double processDeadTime, double tp_lin) {
        double maxOut;
        double in = 0.0;
        double tryIn;
        double icr_max_out;
        double b2m4ac;
        int64_t div;
        int64_t one = 1;

        if (tp_lin == 0) {
            // If no energy dependency, fall back to original formula
            maxOut = 1 / processDeadTime * exp(-1.0);
            icr_max_out = 1 / processDeadTime;
        } else {
            // See if the curve has a maximum turning point, (depends on tp_lin)
            b2m4ac = processDeadTime * processDeadTime + 8 * tp_lin;
            if (b2m4ac > 0) {
                // If it does, assume OCR and ICR will be less than this maximum, so curve is monotonic
                icr_max_out = (-processDeadTime + sqrt(b2m4ac)) / (4 * tp_lin);
                maxOut = icr_max_out * exp(-icr_max_out * (processDeadTime + tp_lin * icr_max_out));
            } else {
                /* If not, allow curve to be used up to the OCR = ICR point. */
                /* first try assumption that tp_all adjustment is small */
                maxOut = 1 / processDeadTime * exp(-1.0);
                icr_max_out = 1 / processDeadTime;
                icr_max_out *= 1.8; // Allow more range, but force first trial to be lower than the maximum rate point.
                if (ffr > maxOut) {
                    maxOut = ffr;
                    icr_max_out = maxOut * 2;
                }
            }
        }
        if (ffr < 0) {
            in = 0.0;
        } else if (ffr > maxOut) {
            in = icr_max_out;
        } else {
            // Gives 50 bit absolute precision, but note low count rate
            int i;
            for (i = 1; i <= 56; i++) {
                div = one << i;
                tryIn = in + icr_max_out / div;
                if (ffr >= tryIn * exp(-tryIn * (processDeadTime + tp_lin * tryIn)))
                    in = tryIn;
            }
        }
        return in;
    }


    XspressProcessPlugin::XspressProcessPlugin() {
        // Setup logging for the class
        logger_ = Logger::getLogger("FP.XspressProcessPlugin");
        logger_->setLevel(Level::getAll());
        LOG4CXX_TRACE(logger_, "XspressProcessPlugin constructor.");
        dtcParams = NULL;
        dtcFlags = NULL;

    }

    XspressProcessPlugin::~XspressProcessPlugin() {
        LOG4CXX_TRACE(logger_, "XspressProcessPlugin destructor.");
    }

     void XspressProcessPlugin::configure(OdinData::IpcMessage& config, OdinData::IpcMessage& reply) {
         if (dtcParams != NULL) delete [] dtcParams;
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
     }

    // Version functions
    int XspressProcessPlugin::get_version_major() {
        return XSPRESS_DETECTOR_VERSION_MAJOR;
    }

    int XspressProcessPlugin::get_version_minor() {
        return XSPRESS_DETECTOR_VERSION_MINOR;
    }

    int XspressProcessPlugin::get_version_patch() {
        return XSPRESS_DETECTOR_VERSION_PATCH;
    }

    std::string XspressProcessPlugin::get_version_short() {
        return XSPRESS_DETECTOR_VERSION_STR_SHORT;
    }

    std::string XspressProcessPlugin::get_version_long() {
        return XSPRESS_DETECTOR_VERSION_STR;
    }

     void XspressProcessPlugin::deadtimeCorrectScalers(uint32_t numChans, uint32_t *iSCA, double *dSCA, double *factors,
                                                      double *corrected, double clockPeriod) {
         for (int chan = 0; chan < numChans; chan ++)
         {
             for (int k = XSP3_SW_NUM_SCALERS*chan; k < XSP3_SW_NUM_SCALERS*(chan + 1); k++)
             {
                 if (k == XSP3_SCALER_INWINDOW0 || k == XSP3_SCALER_INWINDOW1)
                 {
                     dSCA[k] = iSCA[k] * factors[chan];
                 }
                 else if ((k == XSP3_SCALER_ALLEVENT && !(this->dtcFlags[chan] & XSP3_DTC_USE_GOOD_EVENT)) ||
                            (k == XSP3_SCALER_ALLGOOD && (this->dtcFlags[chan] & XSP3_DTC_USE_GOOD_EVENT)))
                 {
                     dSCA[k] = corrected[chan] * (double) iSCA[XSP3_SW_NUM_SCALERS*chan + XSP3_SCALER_TIME] * clockPeriod;
                 }
                 else
                 {
                     dSCA[k] = iSCA[k];
                 }
             }
         }
     };

     void XspressProcessPlugin::calculateDeadtimeCorrection(uint32_t numChans, double clockPeriod, double deadtimeEnergy, uint32_t *sca, double *factors, double *corrected) {
         for (int chan = 0; chan < numChans; chan ++)
         {
             double factor;
             double *chanDtc = this->dtcParams + XSP3_NUM_DTC_FLOAT_PARAMS * chan;
             uint32_t allGood = sca[chan * XSP3_SW_NUM_SCALERS + XSP3_SCALER_ALLGOOD];
             uint32_t allEvent = sca[chan * XSP3_SW_NUM_SCALERS + XSP3_SCALER_ALLEVENT];
             uint32_t time = sca[chan * XSP3_SW_NUM_SCALERS + XSP3_SCALER_TIME];
             uint32_t resetTicks = sca[chan * XSP3_SW_NUM_SCALERS + XSP3_SCALER_RESETTICKS];
             double processDeadTimeAllEvent = chanDtc[XSP3_DTC_AEO] + chanDtc[XSP3_DTC_AEG] * deadtimeEnergy;
             double processDeadTimeInWindowEvent = chanDtc[XSP3_DTC_IWO] + chanDtc[XSP3_DTC_IWG] * deadtimeEnergy;
             double processDeadTimeAllEventRate = chanDtc[XSP3_DTC_AERO] + chanDtc[XSP3_DTC_AERG] * deadtimeEnergy;
             double processDeadTimeInWindowEventRate = chanDtc[XSP3_DTC_IWRO] + chanDtc[XSP3_DTC_IWRG] * deadtimeEnergy;
             double all = (this->dtcFlags[chan] & XSP3_DTC_USE_GOOD_EVENT) ? allGood : allEvent;
             /* Copied from xsp3_dtc in libxspress3/xspress3_dtc.c */
             {
                 double dt = (time - resetTicks) * clockPeriod;
                 double measuredRate = all / dt;
                 // calculate the input corrected count rate
                 corrected[chan] = xsp3_correct_tp_lin(measuredRate, processDeadTimeAllEvent,
                                                       processDeadTimeAllEventRate);
                 // calculate dead time correction factor to be applied to the in-window scaler
                 factor = time * clockPeriod / dt;
                 factor *= 1.0 / exp(-corrected[chan] * 2 *
                                     (processDeadTimeInWindowEvent +
                                      corrected[chan] * processDeadTimeInWindowEventRate));
                 // need a sensible number if there were zeroes in the reading
                 if (factor == DBL_MIN || factor == DBL_MAX || factor == INFINITY || isnan(factor)) {
                     factor = 1.0;
                 }
             }
             factors[chan] = factor;
         }
     }

    void XspressProcessPlugin::process_frame(boost::shared_ptr <Frame> frame) {
        char* frameBytes = static_cast<char *>(frame->get_data_ptr());	
        FrameHeader *header_ = reinterpret_cast<FrameHeader *>(
            frameBytes);
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

        FrameProcessor::DataType scaDataType = this->dtcEnabled ? raw_64bit : XSPRESS_DATA_TYPE;

        dimensions_t scaler_dims;
        scaler_dims.push_back(header_->numChannels);
        scaler_dims.push_back(XSP3_SW_NUM_SCALERS);
        FrameMetaData scaler_metadata(header_->frameNumber, "sca",
            scaDataType, frame->meta_data().get_acquisition_ID(),
            scaler_dims);
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

}
