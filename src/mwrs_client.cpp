/**
 * @file    mwrs_client.cpp
 * @author  Bastien Brunnenstein
 * @license BSD 3-Clause
 */

#include <mwrs.h>
#include "mwrs_messages.hpp"


 // API implementation

int mwrs_res_is_valid(mwrs_res * res)
{
  return res != (mwrs_res*)0;
}

int mwrs_watcher_is_valid(mwrs_watcher * watcher)
{
  return watcher != (mwrs_watcher*)0;
}
