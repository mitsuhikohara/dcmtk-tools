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


#include "dcmtk/config/osconfig.h"    /* make sure OS specific configuration is included first */

#include "dmppsscp.h"
#include "dcmtk/dcmnet/diutil.h"

// implementation of the main interface class

DcmMPPSSCP::DcmMPPSSCP()
  : DcmSCP()
{
    // make sure that the SCP at least supports C-ECHO with default transfer syntax
    OFList<OFString> transferSyntaxes;
    transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
    addPresentationContext(UID_VerificationSOPClass, transferSyntaxes);
    // add MPPS (N-CREATE,N-SET) support
    addPresentationContext(UID_ModalityPerformedProcedureStepSOPClass, transferSyntaxes);
}


DcmMPPSSCP::~DcmMPPSSCP()
{
}

// protected methods

OFCondition DcmMPPSSCP::handleIncomingCommand(T_DIMSE_Message *incomingMsg,
                                                 const DcmPresentationContextInfo &presInfo)
{
    OFCondition status = EC_IllegalParameter;
    if (incomingMsg != NULL)
    {
        // check whether we've received a supported command
        if (incomingMsg->CommandField == DIMSE_C_ECHO_RQ)
        {
            // handle incoming C-ECHO request
            status = handleECHORequest(incomingMsg->msg.CEchoRQ, presInfo.presentationContextID);
        }
        else if (incomingMsg->CommandField == DIMSE_N_CREATE_RQ)
        {
            // handle incoming N-CREATE request
            T_DIMSE_N_CreateRQ &createReq = incomingMsg->msg.NCreateRQ;
            Uint16 rspStatusCode = STATUS_N_NoSuchAttribute;

            DcmFileFormat fileformat;
            DcmDataset *reqDataset = fileformat.getDataset();

            // receive dataset in memory
            status = receiveCREATERequest(createReq, presInfo.presentationContextID, reqDataset);
            if (status.good())
            {
                // output debug message that dataset is not stored
                rspStatusCode = STATUS_Success;
            }
            else
            {
                // output debug message that dataset is not stored
                DCMNET_ERROR("received dataset is not appropriate");
                rspStatusCode = STATUS_N_AttributeListError;
            }

            status = sendCREATEResponse(presInfo.presentationContextID, createReq, rspStatusCode);

        }
        else if (incomingMsg->CommandField == DIMSE_N_SET_RQ)
        {
            // handle incoming N-SET request
            T_DIMSE_N_SetRQ &setReq = incomingMsg->msg.NSetRQ;
            Uint16 rspStatusCode = STATUS_N_NoSuchAttribute ;

            DcmFileFormat fileformat;
            DcmDataset *reqDataset = fileformat.getDataset();

            // receive dataset in memory
            status = receiveSETRequest(setReq, presInfo.presentationContextID, reqDataset);
            if (status.good())
            {
                // output debug message that dataset is not stored
                rspStatusCode = STATUS_Success;
            }
            else
            {
                // output debug message that dataset is not stored
                DCMNET_ERROR("received dataset is not appropriate");
                rspStatusCode = STATUS_N_AttributeListError;
            }

            status = sendSETResponse(presInfo.presentationContextID, setReq, rspStatusCode);

        } else {
            // unsupported command
            OFString tempStr;
            DCMNET_ERROR("cannot handle this kind of DIMSE command (0x"
                << STD_NAMESPACE hex << STD_NAMESPACE setfill('0') << STD_NAMESPACE setw(4)
                << OFstatic_cast(unsigned int, incomingMsg->CommandField)
                << "), we are a Storage SCP only");
            DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, *incomingMsg, DIMSE_INCOMING));
            // TODO: provide more information on this error?
            status = DIMSE_BADCOMMANDTYPE;
        }
    }
    return status;
}

OFCondition DcmMPPSSCP::receiveCREATERequest(T_DIMSE_N_CreateRQ &reqMessage,
                                        const T_ASC_PresentationContextID presID,
                                        DcmDataset *&reqDataset)
{
  OFCondition cond;
  OFString tempStr;
  T_ASC_PresentationContextID presIDdset;
  // Remember the passed dataset pointer
  DcmDataset *dataset = reqDataset;

  // Dump debug information
  if (DCM_dcmnetLogger.isEnabledFor(OFLogger::DEBUG_LOG_LEVEL))
    DCMNET_INFO("Received N-CREATE Request");
  else
    DCMNET_INFO("Received N-CREATE Request (MsgID " << reqMessage.MessageID << ")");

  // Check if dataset is announced correctly
  if (reqMessage.DataSetType == DIMSE_DATASET_NULL)
  {
    DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, reqMessage, DIMSE_INCOMING, NULL, presID));
    DCMNET_ERROR("Received N-CREATE request but no dataset announced, aborting");
    return DIMSE_BADMESSAGE;
  }

  // Receive dataset (in memory)
  cond = receiveDIMSEDataset(&presIDdset, &dataset);
  if (cond.bad())
  {
    DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, reqMessage, DIMSE_INCOMING, NULL, presID));
    DCMNET_ERROR("Unable to receive N-CREATE dataset on presentation context " << OFstatic_cast(unsigned int, presID));
    return cond;
  }

  DCMNET_INFO(DIMSE_dumpMessage(tempStr, reqMessage, DIMSE_INCOMING, dataset, presID));

  // Output request message only if trace level is enabled
  if (DCM_dcmnetLogger.isEnabledFor(OFLogger::TRACE_LOG_LEVEL))
    DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, reqMessage, DIMSE_INCOMING, dataset, presID));
  else
    DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, reqMessage, DIMSE_INCOMING, NULL, presID));

  // Compare presentation context ID of command and data set
  if (presIDdset != presID)
  {
    DCMNET_ERROR("Presentation Context ID of command (" << OFstatic_cast(unsigned int, presID)
      << ") and data set (" << OFstatic_cast(unsigned int, presIDdset) << ") differs");
    if (dataset != reqDataset)
    {
      // Free memory allocated by receiveDIMSEDataset()
      delete dataset;
    }
    return makeDcmnetCondition(DIMSEC_INVALIDPRESENTATIONCONTEXTID, OF_error,
      "DIMSE: Presentation Contexts of Command and Data Set differ");
  }

  // Set return value
  reqDataset = dataset;

  return cond;
}

OFCondition DcmMPPSSCP::sendCREATEResponse(T_ASC_PresentationContextID presID,
                                      const T_DIMSE_N_CreateRQ &reqMessage,
                                      const Uint16 rspStatusCode)
{
  OFCondition cond;
  OFString tempStr;

  // Send back response
  T_DIMSE_Message response;
  // Make sure everything is zeroed (especially options)
  bzero((char*)&response, sizeof(response));
  T_DIMSE_N_CreateRSP &createRsp = response.msg.NCreateRSP;
  response.CommandField = DIMSE_N_CREATE_RSP;
  createRsp.MessageIDBeingRespondedTo = reqMessage.MessageID;
  createRsp.DimseStatus = rspStatusCode;
  createRsp.DataSetType = DIMSE_DATASET_NULL;
  // Always send the optional fields "Affected SOP Class UID" and "Affected SOP Instance UID"
  createRsp.opts = O_STORE_AFFECTEDSOPCLASSUID | O_STORE_AFFECTEDSOPINSTANCEUID;
  OFStandard::strlcpy(createRsp.AffectedSOPClassUID, reqMessage.AffectedSOPClassUID, sizeof(createRsp.AffectedSOPClassUID));
  OFStandard::strlcpy(createRsp.AffectedSOPInstanceUID, reqMessage.AffectedSOPInstanceUID, sizeof(createRsp.AffectedSOPInstanceUID));

  if (DCM_dcmnetLogger.isEnabledFor(OFLogger::DEBUG_LOG_LEVEL))
  {
    DCMNET_INFO("Sending N-CREATE Response");
    DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, response, DIMSE_OUTGOING, NULL, presID));
  } else {
    DCMNET_INFO("Sending N-CREATE Response (" << DU_ncreateStatusString(rspStatusCode) << ")");
  }

  // Send response message
  cond = sendDIMSEMessage(presID, &response, NULL /* dataObject */, NULL);
  if (cond.bad())
  {
    DCMNET_ERROR("Failed sending N-CREATE response: " << DimseCondition::dump(tempStr, cond));
  }

  return cond;

}

OFCondition DcmMPPSSCP::receiveSETRequest(T_DIMSE_N_SetRQ &reqMessage,
                                        const T_ASC_PresentationContextID presID,
                                        DcmDataset *&reqDataset)
{
  OFCondition cond;
  OFString tempStr;
  T_ASC_PresentationContextID presIDdset;
  // Remember the passed dataset pointer
  DcmDataset *dataset = reqDataset;

  // Dump debug information
  if (DCM_dcmnetLogger.isEnabledFor(OFLogger::DEBUG_LOG_LEVEL))
    DCMNET_INFO("Received N-SET Request");
  else
    DCMNET_INFO("Received N-SET Request (MsgID " << reqMessage.MessageID << ")");

  // Check if dataset is announced correctly
  if (reqMessage.DataSetType == DIMSE_DATASET_NULL)
  {
    DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, reqMessage, DIMSE_INCOMING, NULL, presID));
    DCMNET_ERROR("Received N-SET request but no dataset announced, aborting");
    return DIMSE_BADMESSAGE;
  }

  // Receive dataset (in memory)
  cond = receiveDIMSEDataset(&presIDdset, &dataset);
  if (cond.bad())
  {
    DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, reqMessage, DIMSE_INCOMING, NULL, presID));
    DCMNET_ERROR("Unable to receive N-SET dataset on presentation context " << OFstatic_cast(unsigned int, presID));
    return cond;
  }

  DCMNET_INFO(DIMSE_dumpMessage(tempStr, reqMessage, DIMSE_INCOMING, dataset, presID));

  // Output request message only if trace level is enabled
  if (DCM_dcmnetLogger.isEnabledFor(OFLogger::TRACE_LOG_LEVEL))
    DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, reqMessage, DIMSE_INCOMING, dataset, presID));
  else
    DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, reqMessage, DIMSE_INCOMING, NULL, presID));

  // Compare presentation context ID of command and data set
  if (presIDdset != presID)
  {
    DCMNET_ERROR("Presentation Context ID of command (" << OFstatic_cast(unsigned int, presID)
      << ") and data set (" << OFstatic_cast(unsigned int, presIDdset) << ") differs");
    if (dataset != reqDataset)
    {
      // Free memory allocated by receiveDIMSEDataset()
      delete dataset;
    }
    return makeDcmnetCondition(DIMSEC_INVALIDPRESENTATIONCONTEXTID, OF_error,
      "DIMSE: Presentation Contexts of Command and Data Set differ");
  }

  // Set return value
  reqDataset = dataset;

  return cond;
}

OFCondition DcmMPPSSCP::sendSETResponse(T_ASC_PresentationContextID presID,
                                      const T_DIMSE_N_SetRQ &reqMessage,
                                      const Uint16 rspStatusCode)
{
  OFCondition cond;
  OFString tempStr;

  // Send back response
  T_DIMSE_Message response;
  // Make sure everything is zeroed (especially options)
  bzero((char*)&response, sizeof(response));
  T_DIMSE_N_SetRSP &setRsp = response.msg.NSetRSP;
  response.CommandField = DIMSE_N_SET_RSP;
  setRsp.MessageIDBeingRespondedTo = reqMessage.MessageID;
  setRsp.DimseStatus = rspStatusCode;
  setRsp.DataSetType = DIMSE_DATASET_NULL;
  // Always send the optional fields "Affected SOP Class UID" and "Affected SOP Instance UID"
  setRsp.opts = O_STORE_AFFECTEDSOPCLASSUID | O_STORE_AFFECTEDSOPINSTANCEUID;
  OFStandard::strlcpy(setRsp.AffectedSOPClassUID, reqMessage.RequestedSOPClassUID, sizeof(setRsp.AffectedSOPClassUID));
  OFStandard::strlcpy(setRsp.AffectedSOPInstanceUID, reqMessage.RequestedSOPInstanceUID, sizeof(setRsp.AffectedSOPInstanceUID));

  if (DCM_dcmnetLogger.isEnabledFor(OFLogger::DEBUG_LOG_LEVEL))
  {
    DCMNET_INFO("Sending N-SET Response");
    DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, response, DIMSE_OUTGOING, NULL, presID));
  } else {
    DCMNET_INFO("Sending N-SET Response (" << DU_nsetStatusString(rspStatusCode) << ")");
  }

  // Send response message
  cond = sendDIMSEMessage(presID, &response, NULL /* dataObject */, NULL);
  if (cond.bad())
  {
    DCMNET_ERROR("Failed sending N-SET response: " << DimseCondition::dump(tempStr, cond));
  }

  return cond;

}

