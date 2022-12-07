/*
 * LibXspressWrapper.cpp
 *
 *  Created on: 24 Sep 2021
 *      Author: Diamond Light Source
 */

#include <stdio.h>
#include "dirent.h"

#include "LibXspressWrapper.h"
#include "DebugLevelLogger.h"

// handle to xspress from libxspress
extern XSP3Path Xsp3Sys[];


namespace Xspress
{
const int N_RESGRADES = 16;

//Definitions of static class data members
const int ILibXspress::runFlag_MCA_SPECTRA_ = 0;
const int ILibXspress::runFlag_SCALERS_ONLY_ = 1;
const int ILibXspress::runFlag_PLAYB_MCA_SPECTRA_ = 2;

void ILibXspress::setErrorString(const std::string& error)
{
  LOG4CXX_ERROR(logger_, error);
  error_string_ = error;
}

std::string ILibXspress::getErrorString()
{
  return error_string_;
}


} /* namespace Xspress */
