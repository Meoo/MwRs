/**
 * @file    mwrs_messages.hpp
 * @author  Bastien Brunnenstein
 * @license BSD 3-Clause
 */


#ifndef MWRS_MESSAGES__HEADER_GUARD
#define MWRS_MESSAGES__HEADER_GUARD

#pragma pack(push, 1)

enum mwrs_server_message_type
{

};

struct mwrs_server_message
{
  mwrs_server_message_type type;
};

//

enum mwrs_client_message_type
{

};

struct mwrs_client_message
{
  mwrs_client_message_type type;
};

#pragma pack(pop)

#endif // MWRS_MESSAGES__HEADER_GUARD
