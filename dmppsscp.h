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

#ifndef DMPPSSCP_H
#define DMPPSSCP_H

#include "dcmtk/config/osconfig.h"  /* make sure OS specific configuration is included first */

#include "dcmtk/ofstd/offname.h"    /* for OFFilenameCreator */
#include "dcmtk/dcmnet/scp.h"       /* for base class DcmSCP */


/*---------------------*
 *  class declaration  *
 *---------------------*/

/** Interface class for a Modality Performed Procedure Step Service Class Provider (SCP).
 *  This class supports N-CREATE, N-SET and C-ECHO messages as an SCP.
 *  @note The current implementation logs DIMSE contens of N-CREATE and N-SET as INFO LOG LEVEL
 */
class DCMTK_DCMNET_EXPORT DcmMPPSSCP : public DcmSCP
{

  public:

    /** default constructor
     */
    DcmMPPSSCP();

    /** destructor
     */
    virtual ~DcmMPPSSCP();

  protected:

    /** handler that is called for each incoming command message.  This handler supports
     *  C-ECHO, N-CREATE and N-SET requests.  All other messages will be reported as an error.
     *  @param  incomingMsg  pointer to data structure containing the DIMSE message
     *  @param  presInfo     additional information on the Presentation Context used
     *  @return status, EC_Normal if successful, an error code otherwise
     */
    virtual OFCondition handleIncomingCommand(T_DIMSE_Message *incomingMsg,
                                              const DcmPresentationContextInfo &presInfo);

  /** Receive N-CREATE request (and store accompanying dataset in memory).
   *  @param reqMessage [in]    The N-CREATE request message that was received
   *  @param presID     [in]    The presentation context to be used. By default, the
   *                            presentation context of the request is used.
   *  @param reqDataset [inout] Pointer to data structure where the received dataset
   *                            should be stored. If NULL, a new dataset is created,
   *                            which has to be deleted by the caller.
   *  @return status, EC_Normal if successful, an error code otherwise
   */
  virtual OFCondition receiveCREATERequest(T_DIMSE_N_CreateRQ &reqMessage,
                                          const T_ASC_PresentationContextID presID,
                                          DcmDataset *&reqDataset);

  /** Respond to the N-CREATE request
   *  @param presID        [in] The presentation context ID to respond to
   *  @param reqMessage    [in] The N-CREATE request that should be responded to
   *  @param rspStatusCode [in] The response status code. 0 means success,
   *                            others can found in the DICOM standard.
   *  @return EC_Normal, if responding was successful, an error code otherwise
   */
  virtual OFCondition sendCREATEResponse(const T_ASC_PresentationContextID presID,
                                        const T_DIMSE_N_CreateRQ &reqMessage,
                                        const Uint16 rspStatusCode);

  /** Receive N-SET request (and store accompanying dataset in memory).
   *  @param reqMessage [in]    The N-SET request message that was received
   *  @param presID     [in]    The presentation context to be used. By default, the
   *                            presentation context of the request is used.
   *  @param reqDataset [inout] Pointer to data structure where the received dataset
   *                            should be stored. If NULL, a new dataset is created,
   *                            which has to be deleted by the caller.
   *  @return status, EC_Normal if successful, an error code otherwise
   */
  virtual OFCondition receiveSETRequest(T_DIMSE_N_SetRQ &reqMessage,
                                          const T_ASC_PresentationContextID presID,
                                          DcmDataset *&reqDataset);

  /** Respond to the N-SET request 
   *  @param presID        [in] The presentation context ID to respond to
   *  @param reqMessage    [in] The N-SET request that should be responded to
   *  @param rspStatusCode [in] The response status code. 0 means success,
   *                            others can found in the DICOM standard.
   *  @return EC_Normal, if responding was successful, an error code otherwise
   */
  virtual OFCondition sendSETResponse(const T_ASC_PresentationContextID presID,
                                        const T_DIMSE_N_SetRQ &reqMessage,
                                        const Uint16 rspStatusCode);

private:

    // private undefined copy constructor
    DcmMPPSSCP(const DcmMPPSSCP &);

    // private undefined assignment operator
    DcmMPPSSCP &operator=(const DcmMPPSSCP &);
};

#endif // DMPPSSCP_H
