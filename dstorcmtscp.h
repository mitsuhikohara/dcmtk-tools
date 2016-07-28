/*
 *
 *  Copyright (C) 2013-2014, OFFIS e.V.
 *  All rights reserved.  See COPYRIGHT file for details.
 *
 *  This software and supporting documentation were developed by
 *
 *    OFFIS e.V.
 *    R&D Division Health
 *    Escherweg 2
 *    D-26121 Oldenburg, Germany
 *
 *
 *  Module:  dcmnet
 *
 *  Author:  Joerg Riesmeier
 *
 *  Purpose: DICOM Storage Service Class Provider (SCP)
 *
 */

#ifndef DSTORCMTSCP_H
#define DSTORCMTSCP_H

#include "dcmtk/config/osconfig.h"  /* make sure OS specific configuration is included first */

#include "dcmtk/ofstd/offname.h"    /* for OFFilenameCreator */
#include "dcmtk/dcmnet/scp.h"       /* for base class DcmSCP */


/*---------------------*
 *  class declaration  *
 *---------------------*/

/** Interface class for a Storage Commiment Push Model Service Class Provider (SCP).
 *  This class supports N-ACTION, N-EVENT-REPORT  and C-ECHO messages as an SCP.
 *  @note The current implementation send N-EVENT-REPORT in the same association as
 *  N-ACTION
 */
class DCMTK_DCMNET_EXPORT DcmStorCmtSCP : public DcmSCP
{

  public:

    /** default constructor
     */
    DcmStorCmtSCP();

    /** destructor
     */
    virtual ~DcmStorCmtSCP();

  protected:

    /** handler that is called for each incoming command message.  This handler supports
     *  C-ECHO, N-ACTION requests.  All other messages will be reported as an error.
     *  @param  incomingMsg  pointer to data structure containing the DIMSE message
     *  @param  presInfo     additional information on the Presentation Context used
     *  @return status, EC_Normal if successful, an error code otherwise
     */
    virtual OFCondition handleIncomingCommand(T_DIMSE_Message *incomingMsg,
                                              const DcmPresentationContextInfo &presInfo);

  /** Receive N-ACTION request (and store accompanying dataset in memory).
   *  @param reqMessage [in]    The N-ACTION request message that was received
   *  @param presID     [in]    The presentation context to be used. By default, the
   *                            presentation context of the request is used.
   *  @param reqDataset [inout] Pointer to data structure where the received dataset
   *                            should be stored. If NULL, a new dataset is created,
   *                            which has to be deleted by the caller.
   *  @return status, EC_Normal if successful, an error code otherwise
   */
  virtual OFCondition receiveACTIONRequest(T_DIMSE_N_ActionRQ &reqMessage,
                                          const T_ASC_PresentationContextID presID,
                                          DcmDataset *&reqDataset);

  /** Respond to the N-ACTION request
   *  @param presID        [in] The presentation context ID to respond to
   *  @param reqMessage    [in] The N-ACTION request that should be responded to
   *  @param rspStatusCode [in] The response status code. 0 means success,
   *                            others can found in the DICOM standard.
   *  @return EC_Normal, if responding was successful, an error code otherwise
   */
  virtual OFCondition sendACTIONResponse(const T_ASC_PresentationContextID presID,
                                        const T_DIMSE_N_ActionRQ &reqMessage,
                                        const Uint16 rspStatusCode);

  /** Send the N-EVENT-REPORT request (with dataset)
   *  @param presID         [in] The presentation context ID to respond to
   *  @param messageID      [in] The message ID to send
   *  @param sopClassUID    [in] The affected SOP class UID
   *  @param sopInstanceUID [in] The affected SOP instance UID
   *  @param reqDataset     [in] The Dataset to send  (if desired).
   *  @return EC_Normal, if responding was successful, an error code otherwise
   */
  virtual OFCondition sendEVENTREPORTRequest(const T_ASC_PresentationContextID presID,
                                        const Uint16 messageID,
                                        const OFString &sopClassUID,
                                        const OFString &sopInstanceUID,
                                        DcmDataset *reqDataset = NULL);

  /** Receive N-EVENT-REPORT responset
   *  @param respMessage [in]    The N-EVENT-REPORT response message that was received
   *  @param presID     [in]    The presentation context to be used. By default, the
   *                            presentation context of the request is used.
   *  @return status, EC_Normal if successful, an error code otherwise
   */
  virtual OFCondition receiveEVENTREPORTResponse(T_DIMSE_N_EventReportRSP &respMessage,
                                          const T_ASC_PresentationContextID presID );


private:

    // private undefined copy constructor
    DcmStorCmtSCP(const DcmStorCmtSCP &);

    // private undefined assignment operator
    DcmStorCmtSCP &operator=(const DcmStorCmtSCP &);

    /** Returns next available message ID free to be used by SCU
    *  @return Next free message ID
    */
    Uint16 eventReportMsgID;

};

#endif // DSTORCMTSCP_H
