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

#include "dstorcmtscp.h"
#include "dcmtk/dcmnet/diutil.h"

// implementation of the main interface class

DcmStorCmtSCP::DcmStorCmtSCP()
  : DcmSCP(), 
    eventReportMsgID(1)
{
    // make sure that the SCP at least supports C-ECHO with default transfer syntax
    OFList<OFString> transferSyntaxes;
    transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
    addPresentationContext(UID_VerificationSOPClass, transferSyntaxes);
    // add Storage Commitment support
    addPresentationContext(UID_StorageCommitmentPushModelSOPClass, transferSyntaxes);
}


DcmStorCmtSCP::~DcmStorCmtSCP()
{
}

// protected methods

OFCondition DcmStorCmtSCP::handleIncomingCommand(T_DIMSE_Message *incomingMsg,
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
        else if (incomingMsg->CommandField == DIMSE_N_ACTION_RQ)
        {
            // handle incoming N-ACTION request
            T_DIMSE_N_ActionRQ &actionReq = incomingMsg->msg.NActionRQ;
            Uint16 rspStatusCode = STATUS_N_NoSuchAttribute;

            DcmFileFormat fileformat;
            DcmDataset *reqDataset = fileformat.getDataset();

            // receive dataset in memory
            status = receiveACTIONRequest(actionReq, presInfo.presentationContextID, reqDataset);
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

            DcmDataset *reportDataset = (DcmDataset *)reqDataset->clone();

            status = sendACTIONResponse(presInfo.presentationContextID, actionReq, rspStatusCode);

            // current implementation send N-EVENT-REPORT requiest in the same association
            if (status.good())
            {
                 Uint16 messageID = eventReportMsgID++;
                 OFString sopClassUID = actionReq.RequestedSOPClassUID;
                 OFString sopInstanceUID = actionReq.RequestedSOPInstanceUID;
                 status = sendEVENTREPORTRequest(presInfo.presentationContextID,
                                      messageID, sopClassUID, sopInstanceUID, reportDataset);
            }
        }
        else if (incomingMsg->CommandField == DIMSE_N_EVENT_REPORT_RSP)
        {
            // handle incoming N-EVENT-REPORT response
            T_DIMSE_N_EventReportRSP &eventReportRSP = incomingMsg->msg.NEventReportRSP;

            status = receiveEVENTREPORTResponse(eventReportRSP, presInfo.presentationContextID);

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

OFCondition DcmStorCmtSCP::receiveACTIONRequest(T_DIMSE_N_ActionRQ &reqMessage,
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
    DCMNET_INFO("Received N-ACTION Request");
  else
    DCMNET_INFO("Received N-ACTION Request (MsgID " << reqMessage.MessageID << ")");

  // Check if dataset is announced correctly
  if (reqMessage.DataSetType == DIMSE_DATASET_NULL)
  {
    DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, reqMessage, DIMSE_INCOMING, NULL, presID));
    DCMNET_ERROR("Received N-ACTION request but no dataset announced, aborting");
    return DIMSE_BADMESSAGE;
  }

  // Receive dataset (in memory)
  cond = receiveDIMSEDataset(&presIDdset, &dataset);
  if (cond.bad())
  {
    DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, reqMessage, DIMSE_INCOMING, NULL, presID));
    DCMNET_ERROR("Unable to receive N-ACTION dataset on presentation context " << OFstatic_cast(unsigned int, presID));
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

OFCondition DcmStorCmtSCP::sendACTIONResponse(T_ASC_PresentationContextID presID,
                                      const T_DIMSE_N_ActionRQ &reqMessage,
                                      const Uint16 rspStatusCode)
{
  OFCondition cond;
  OFString tempStr;

  // Send back response
  T_DIMSE_Message response;
  // Make sure everything is zeroed (especially options)
  bzero((char*)&response, sizeof(response));
  T_DIMSE_N_ActionRSP &actionRsp = response.msg.NActionRSP;
  response.CommandField = DIMSE_N_ACTION_RSP;
  actionRsp.MessageIDBeingRespondedTo = reqMessage.MessageID;
  actionRsp.DimseStatus = rspStatusCode;
  actionRsp.DataSetType = DIMSE_DATASET_NULL;
  // Always send the optional fields "Affected SOP Class UID" and "Affected SOP Instance UID"
  actionRsp.opts = O_STORE_AFFECTEDSOPCLASSUID | O_STORE_AFFECTEDSOPINSTANCEUID;
  OFStandard::strlcpy(actionRsp.AffectedSOPClassUID, reqMessage.RequestedSOPClassUID, sizeof(actionRsp.AffectedSOPClassUID));
  OFStandard::strlcpy(actionRsp.AffectedSOPInstanceUID, reqMessage.RequestedSOPInstanceUID, sizeof(actionRsp.AffectedSOPInstanceUID));

  if (DCM_dcmnetLogger.isEnabledFor(OFLogger::DEBUG_LOG_LEVEL))
  {
    DCMNET_INFO("Sending N-ACTION Response");
    DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, response, DIMSE_OUTGOING, NULL, presID));
  } else {
    DCMNET_INFO("Sending N-ACTION Response (" << DU_ncreateStatusString(rspStatusCode) << ")");
  }

  // Send response message
  cond = sendDIMSEMessage(presID, &response, NULL /* dataObject */, NULL);
  if (cond.bad())
  {
    DCMNET_ERROR("Failed sending N-ACTION response: " << DimseCondition::dump(tempStr, cond));
  }

  return cond;

}

OFCondition DcmStorCmtSCP::sendEVENTREPORTRequest(const T_ASC_PresentationContextID presID,
                                      const Uint16 messageID,
                                      const OFString &sopClassUID,
                                      const OFString &sopInstanceUID,
                                      DcmDataset *reqDataset)
{
  OFCondition cond;
  OFString tempStr;

  // Send request
  T_DIMSE_Message request;
  // Make sure everything is zeroed (especially options)
  bzero((char*)&request, sizeof(request));
  T_DIMSE_N_EventReportRQ &eventReportRq = request.msg.NEventReportRQ;
  request.CommandField = DIMSE_N_EVENT_REPORT_RQ;
  eventReportRq.DataSetType = DIMSE_DATASET_PRESENT;
  eventReportRq.EventTypeID = 1;
  OFStandard::strlcpy(eventReportRq.AffectedSOPClassUID, sopClassUID.c_str(), sizeof(eventReportRq.AffectedSOPClassUID));
  OFStandard::strlcpy(eventReportRq.AffectedSOPInstanceUID, sopInstanceUID.c_str(), sizeof(eventReportRq.AffectedSOPInstanceUID));


  if (DCM_dcmnetLogger.isEnabledFor(OFLogger::DEBUG_LOG_LEVEL))
  {
    DCMNET_INFO("Sending N-EVENT-REPORT Request");
    DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, request, DIMSE_OUTGOING, NULL, presID));
  } else {
    DCMNET_INFO("Sending N-EVENT-REPORT Request");
  }

  if (reqDataset == NULL) 
  {
    DCMNET_ERROR("reqDataset is NULL");
  }

  // Send response message
  cond = sendDIMSEMessage(presID, &request, reqDataset, NULL);
  if (cond.bad())
  {
    DCMNET_ERROR("Failed sending N-EVENT-REPORT request: " << DimseCondition::dump(tempStr, cond));
  }

  delete reqDataset;

  return cond;
}

OFCondition DcmStorCmtSCP::receiveEVENTREPORTResponse(T_DIMSE_N_EventReportRSP &respMessage,
                                        const T_ASC_PresentationContextID presID )
{
  OFCondition cond = EC_Normal;
  OFString tempStr;

  // Dump debug information
  DCMNET_INFO("Received N-EVENT-REPORT Response");

  DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, respMessage, DIMSE_INCOMING, NULL, presID));

  return cond;
}

