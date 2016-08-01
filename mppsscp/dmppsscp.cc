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

DcmMppsSCP::DcmMppsSCP():
  m_assoc(NULL),
  m_cfg()
{
    // make sure that the SCP at least supports C-ECHO with default transfer syntax
    OFList<OFString> transferSyntaxes;
    transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
    transferSyntaxes.push_back(UID_BigEndianExplicitTransferSyntax);
    transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
    addPresentationContext(UID_VerificationSOPClass, transferSyntaxes);
    // add MPPS (N-CREATE,N-SET) support
    addPresentationContext(UID_ModalityPerformedProcedureStepSOPClass, transferSyntaxes);

}


DcmMppsSCP::~DcmMppsSCP()
{
  // If there is an open association, drop it and free memory (just to be sure...)
  if (m_assoc)
  {
    dropAndDestroyAssociation();
  }

}

// ----------------------------------------------------------------------------

OFCondition DcmMppsSCP::listen()
{

  // make sure not to let dcmdata remove trailing blank padding or perform other
  // manipulations. We want to see the real data.
  dcmEnableAutomaticInputDataCorrection.set( OFFalse );

  OFCondition cond = EC_Normal;
  // Make sure data dictionary is loaded.
  if( !dcmDataDict.isDictionaryLoaded() )
    DCMNET_WARN("No data dictionary loaded, check environment variable: " << DCM_DICT_ENVIRONMENT_VARIABLE);

#ifndef DISABLE_PORT_PERMISSION_CHECK
#ifdef HAVE_GETEUID
  // If port is privileged we must be as well.
  if( m_cfg->getPort() < 1024 && geteuid() != 0 )
  {
    DCMNET_ERROR("No privileges to open this network port (" << m_cfg->getPort() << ")");
    return NET_EC_InsufficientPortPrivileges;
  }
#endif
#endif

  // Initialize network, i.e. create an instance of T_ASC_Network*.
  T_ASC_Network *network = NULL;
  cond = ASC_initializeNetwork( NET_ACCEPTOR, OFstatic_cast(int, m_cfg->getPort()), m_cfg->getACSETimeout(), &network );
  if( cond.bad() )
    return cond;

  // drop root privileges now and revert to the calling user id (if we are running as setuid root)
  cond = OFStandard::dropPrivileges();
  if (cond.bad())
  {
      DCMNET_ERROR("setuid() failed, maximum number of processes/threads for uid already running.");
      return cond;
  }

  // If we get to this point, the entire initialization process has been completed
  // successfully. Now, we want to start handling all incoming requests. Since
  // this activity is supposed to represent a server process, we do not want to
  // terminate this activity (unless indicated by the stopAfterCurrentAssociation()
  // method). Hence, create an infinite while-loop.
  while( cond.good() && !stopAfterCurrentAssociation() )
  {
    // Wait for an association and handle the requests of
    // the calling applications correspondingly.
    cond = waitForAssociationRQ(network);
  }
  // Drop the network, i.e. free memory of T_ASC_Network* structure. This call
  // is the counterpart of ASC_initializeNetwork(...) which was called above.
  cond = ASC_dropNetwork( &network );
  network = NULL;

  // return ok
  return cond;
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::findPresentationContext(const T_ASC_PresentationContextID presID,
                                     OFString &abstractSyntax,
                                     OFString &transferSyntax)
{
  transferSyntax.clear();
  abstractSyntax.clear();
  if (m_assoc == NULL)
    return;

  DUL_PRESENTATIONCONTEXT *pc;
  LST_HEAD **l;

  /* we look for a presentation context matching
   * both abstract and transfer syntax
   */
  l = &m_assoc->params->DULparams.acceptedPresentationContext;
  pc = (DUL_PRESENTATIONCONTEXT*) LST_Head(l);
  (void)LST_Position(l, (LST_NODE*)pc);
  while (pc)
  {
     if (presID == pc->presentationContextID)
     {
       if (pc->result == ASC_P_ACCEPTANCE)
       {
         // found a match
         transferSyntax = pc->acceptedTransferSyntax;
         abstractSyntax = pc->abstractSyntax;
       }
       break;
     }
     pc = (DUL_PRESENTATIONCONTEXT*) LST_Next(l);
  }
}

DUL_PRESENTATIONCONTEXT* DcmMppsSCP::findPresentationContextID(LST_HEAD *head,
                                                           T_ASC_PresentationContextID presentationContextID)
{
  DUL_PRESENTATIONCONTEXT *pc;
  LST_HEAD **l;
  OFBool found = OFFalse;

  if (head == NULL)
    return NULL;

  l = &head;
  if (*l == NULL)
    return NULL;

  pc = (DUL_PRESENTATIONCONTEXT*) LST_Head(l);
  (void)LST_Position(l, (LST_NODE*)pc);

  while (pc && !found)
  {
    if (pc->presentationContextID == presentationContextID)
    {
      found = OFTrue;
    } else
    {
      pc = (DUL_PRESENTATIONCONTEXT*) LST_Next(l);
    }
  }
  return pc;
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::refuseAssociation(const DcmRefuseReasonType reason)
{
  if (m_assoc == NULL)
  {
    DCMNET_WARN("DcmSCP::refuseAssociation() called but actually no association running, ignoring");
    return;
  }

  T_ASC_RejectParameters rej;

  // dump some information if required
  switch( reason )
  {
    case DCMSCP_TOO_MANY_ASSOCIATIONS:
      DCMNET_INFO("Refusing Association (too many associations)");
      break;
    case DCMSCP_CANNOT_FORK:
      DCMNET_INFO("Refusing Association (cannot fork)");
      break;
    case DCMSCP_BAD_APPLICATION_CONTEXT_NAME:
      DCMNET_INFO("Refusing Association (bad application context)");
      break;
    case DCMSCP_CALLED_AE_TITLE_NOT_RECOGNIZED:
      DCMNET_INFO("Refusing Association (called AE title not recognized)");
      break;
    case DCMSCP_CALLING_AE_TITLE_NOT_RECOGNIZED:
      DCMNET_INFO("Refusing Association (calling AE title not recognized)");
      break;
    case DCMSCP_FORCED:
      DCMNET_INFO("Refusing Association (forced via command line)");
      break;
    case DCMSCP_NO_IMPLEMENTATION_CLASS_UID:
      DCMNET_INFO("Refusing Association (no implementation class UID provided)");
      break;
    case DCMSCP_NO_PRESENTATION_CONTEXTS:
      DCMNET_INFO("Refusing Association (no acceptable presentation contexts)");
      break;
    case DCMSCP_INTERNAL_ERROR:
      DCMNET_INFO("Refusing Association (internal error)");
      break;
    default:
      DCMNET_INFO("Refusing Association (unknown reason)");
      break;
  }

  // Set some values in the reject message depending on the reason
  switch( reason )
  {
    case DCMSCP_TOO_MANY_ASSOCIATIONS:
      rej.result = ASC_RESULT_REJECTEDTRANSIENT;
      rej.source = ASC_SOURCE_SERVICEPROVIDER_PRESENTATION_RELATED;
      rej.reason = ASC_REASON_SP_PRES_LOCALLIMITEXCEEDED;
      break;
    case DCMSCP_CANNOT_FORK:
      DCMNET_INFO("Refusing Association (cannot fork)");
      break;
    case DCMSCP_BAD_APPLICATION_CONTEXT_NAME:
      DCMNET_INFO("Refusing Association (bad application context)");
      break;
    case DCMSCP_CALLED_AE_TITLE_NOT_RECOGNIZED:
      DCMNET_INFO("Refusing Association (called AE title not recognized)");
      break;
    case DCMSCP_CALLING_AE_TITLE_NOT_RECOGNIZED:
      DCMNET_INFO("Refusing Association (calling AE title not recognized)");
      break;
    case DCMSCP_FORCED:
      DCMNET_INFO("Refusing Association (forced via command line)");
      break;
    case DCMSCP_NO_IMPLEMENTATION_CLASS_UID:
      DCMNET_INFO("Refusing Association (no implementation class UID provided)");
      break;
    case DCMSCP_NO_PRESENTATION_CONTEXTS:
      DCMNET_INFO("Refusing Association (no acceptable presentation contexts)");
      break;
    case DCMSCP_INTERNAL_ERROR:
      DCMNET_INFO("Refusing Association (internal error)");
      break;
    default:
      DCMNET_INFO("Refusing Association (unknown reason)");
      break;
  }

  // Set some values in the reject message depending on the reason
  switch( reason )
  {
    case DCMSCP_TOO_MANY_ASSOCIATIONS:
      rej.result = ASC_RESULT_REJECTEDTRANSIENT;
      rej.source = ASC_SOURCE_SERVICEPROVIDER_PRESENTATION_RELATED;
      rej.reason = ASC_REASON_SP_PRES_LOCALLIMITEXCEEDED;
      break;
    case DCMSCP_CANNOT_FORK:
      rej.result = ASC_RESULT_REJECTEDPERMANENT;
      rej.source = ASC_SOURCE_SERVICEPROVIDER_PRESENTATION_RELATED;
      rej.reason = ASC_REASON_SP_PRES_TEMPORARYCONGESTION;
      break;
    case DCMSCP_BAD_APPLICATION_CONTEXT_NAME:
      rej.result = ASC_RESULT_REJECTEDTRANSIENT;
      rej.source = ASC_SOURCE_SERVICEUSER;
      rej.reason = ASC_REASON_SU_APPCONTEXTNAMENOTSUPPORTED;
      break;
    case DCMSCP_CALLED_AE_TITLE_NOT_RECOGNIZED:
      rej.result = ASC_RESULT_REJECTEDPERMANENT;
      rej.source = ASC_SOURCE_SERVICEUSER;
      rej.reason = ASC_REASON_SU_CALLEDAETITLENOTRECOGNIZED;
      break;
    case DCMSCP_CALLING_AE_TITLE_NOT_RECOGNIZED:
      rej.result = ASC_RESULT_REJECTEDPERMANENT;
      rej.source = ASC_SOURCE_SERVICEUSER;
      rej.reason = ASC_REASON_SU_CALLINGAETITLENOTRECOGNIZED;
      break;
    case DCMSCP_FORCED:
    case DCMSCP_NO_IMPLEMENTATION_CLASS_UID:
    case DCMSCP_NO_PRESENTATION_CONTEXTS:
    case DCMSCP_INTERNAL_ERROR:
    default:
      rej.result = ASC_RESULT_REJECTEDPERMANENT;
      rej.source = ASC_SOURCE_SERVICEUSER;
      rej.reason = ASC_REASON_SU_NOREASON;
      break;
  }

  // Reject the association request.
  ASC_rejectAssociation( m_assoc, &rej );

  // Drop and destroy the association.
  dropAndDestroyAssociation();
}

// ----------------------------------------------------------------------------

OFCondition DcmMppsSCP::waitForAssociationRQ(T_ASC_Network *network)
{
  if (network == NULL)
    return ASC_NULLKEY;
  if (m_assoc != NULL)
    return DIMSE_ILLEGALASSOCIATION;

  Uint32 timeout = m_cfg->getConnectionTimeout();

  // Listen to a socket for timeout seconds and wait for an association request
  OFCondition cond = ASC_receiveAssociation( network, &m_assoc, m_cfg->getMaxReceivePDULength(), NULL, NULL, OFFalse,
                                             m_cfg->getConnectionBlockingMode(), OFstatic_cast(int, timeout) );

  // just return, if timeout occurred (DUL_NOASSOCIATIONREQUEST)
  if ( cond == DUL_NOASSOCIATIONREQUEST )
  {
    return EC_Normal;
  }

  // if error occurs close association and return
  if( cond.bad() )
  {
    dropAndDestroyAssociation();
    return EC_Normal;
  }

  return processAssociationRQ();
}


OFCondition DcmMppsSCP::processAssociationRQ()
{
  DcmSCPActionType desiredAction = DCMSCP_ACTION_UNDEFINED;
  if ( (m_assoc == NULL) || (m_assoc->params == NULL) )
    return ASC_NULLKEY;

  // call notifier function
  notifyAssociationRequest(*m_assoc->params, desiredAction);
  if (desiredAction != DCMSCP_ACTION_UNDEFINED)
  {
    if (desiredAction == DCMSCP_ACTION_REFUSE_ASSOCIATION)
    {
      refuseAssociation( DCMSCP_INTERNAL_ERROR );
      dropAndDestroyAssociation();
      return EC_Normal;
    }
    else desiredAction = DCMSCP_ACTION_UNDEFINED; // reset for later use
  }
  // Now we have to figure out if we might have to refuse the association request.
  // This is the case if at least one of five conditions is met:

  // Condition 1: if option "--refuse" is set we want to refuse the association request.
  if( m_cfg->getRefuseAssociation() )
  {
    refuseAssociation( DCMSCP_FORCED );
    dropAndDestroyAssociation();
    return EC_Normal;
  }

  // Condition 2: determine the application context name. If an error occurred or if the
  // application context name is not supported we want to refuse the association request.
  char buf[BUFSIZ];
  OFCondition cond = ASC_getApplicationContextName( m_assoc->params, buf );
  if( cond.bad() || strcmp( buf, DICOM_STDAPPLICATIONCONTEXT ) != 0 )
  {
    refuseAssociation( DCMSCP_BAD_APPLICATION_CONTEXT_NAME );
    dropAndDestroyAssociation();
    return EC_Normal;
  }

  // Condition 3: if the calling or called application entity title is not supported
  // we want to refuse the association request
  if (!checkCalledAETitleAccepted(m_assoc->params->DULparams.calledAPTitle))
  {
    refuseAssociation( DCMSCP_CALLED_AE_TITLE_NOT_RECOGNIZED );
    dropAndDestroyAssociation();
    return EC_Normal;
  }

  if (!checkCallingAETitleAccepted(m_assoc->params->DULparams.callingAPTitle))
  {
    refuseAssociation( DCMSCP_CALLING_AE_TITLE_NOT_RECOGNIZED );
    dropAndDestroyAssociation();
    return EC_Normal;
  }

  /* set our application entity title */
  if (m_cfg->getRespondWithCalledAETitle())
    ASC_setAPTitles(m_assoc->params, NULL, NULL, m_assoc->params->DULparams.calledAPTitle);
  else
    ASC_setAPTitles(m_assoc->params, NULL, NULL, m_cfg->getAETitle().c_str());

  /* If we get to this point the association shall be negotiated.
     Thus, for every presentation context it is checked whether
     it can be accepted. However, this is only a "dry" run, i.e.
     there is not yet sent a response message to the SCU
   */
  cond = negotiateAssociation();
  if( cond.bad() )
  {
    dropAndDestroyAssociation();
    return EC_Normal;
  }

  // Reject association if no presentation context was negotiated
  if( ASC_countAcceptedPresentationContexts( m_assoc->params ) == 0 )
  {
    // Dump some debug information
    OFString tempStr;
    DCMNET_INFO("No Acceptable Presentation Contexts");
    if (m_cfg->getVerbosePCMode())
      DCMNET_INFO(ASC_dumpParameters(tempStr, m_assoc->params, ASC_ASSOC_RJ));
    else
      DCMNET_DEBUG(ASC_dumpParameters(tempStr, m_assoc->params, ASC_ASSOC_RJ));
    refuseAssociation( DCMSCP_NO_PRESENTATION_CONTEXTS );
    dropAndDestroyAssociation();
    return EC_Normal;
  }

  // If the negotiation was successful, accept the association request
  cond = ASC_acknowledgeAssociation( m_assoc );
  if( cond.bad() )
  {
    dropAndDestroyAssociation();
    return EC_Normal;
  }
  notifyAssociationAcknowledge();

  // Dump some debug information
  OFString tempStr;
  DCMNET_INFO("Association Acknowledged (Max Send PDV: " << OFstatic_cast(Uint32, m_assoc->sendPDVLength) << ")");
  if (m_cfg->getVerbosePCMode())
    DCMNET_INFO(ASC_dumpParameters(tempStr, m_assoc->params, ASC_ASSOC_AC));
  else
    DCMNET_DEBUG(ASC_dumpParameters(tempStr, m_assoc->params, ASC_ASSOC_AC));

   // Go ahead and handle the association (i.e. handle the callers requests) in this process
   handleAssociation();
   return EC_Normal;
}

// ----------------------------------------------------------------------------

OFCondition DcmMppsSCP::negotiateAssociation()
{
  // Check whether there is something to negotiate...
  if (m_assoc == NULL)
    return DIMSE_ILLEGALASSOCIATION;

  // Set presentation contexts as defined in association configuration
  OFCondition result = m_cfg->evaluateIncomingAssociation(*m_assoc);
  if (result.bad())
  {
    OFString tempStr;
    DCMNET_ERROR(DimseCondition::dump(tempStr, result));
  }
  return result;
}


// ----------------------------------------------------------------------------

OFCondition DcmMppsSCP::abortAssociation()
{
    OFCondition cond = DIMSE_ILLEGALASSOCIATION;
    // Check whether there is an active association
    if (isConnected())
    {
      // Abort current association
      DCMNET_INFO("Aborting Association (initiated by SCP)");
      cond = ASC_abortAssociation(m_assoc);
      // Notify user in case of error
      if (cond.bad())
      {
        OFString tempStr;
        DCMNET_ERROR("Association Abort Failed: " << DimseCondition::dump(tempStr, cond));
      }
      // Note: association is dropped and memory freed somewhere else
    } else
      DCMNET_WARN("DcmSCP::abortAssociation() called but SCP actually has no association running, ignoring");
    return cond;
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::handleAssociation()
{
  if (m_assoc == NULL)
  {
    DCMNET_WARN("DcmSCP::handleAssociation() called but SCP actually has no association running, ignoring");
    return;
  }

  // Receive a DIMSE command and perform all the necessary actions. (Note that ReceiveAndHandleCommands()
  // will always return a value 'cond' for which 'cond.bad()' will be true. This value indicates that either
  // some kind of error occurred, or that the peer aborted the association (DUL_PEERABORTEDASSOCIATION),
  // or that the peer requested the release of the association (DUL_PEERREQUESTEDRELEASE).) (Also note
  // that ReceiveAndHandleCommands() will never return EC_Normal.)
  OFCondition cond = EC_Normal;
  T_DIMSE_Message message;
  T_ASC_PresentationContextID presID;

  // start a loop to be able to receive more than one DIMSE command
  while( cond.good() )
  {
    // receive a DIMSE command over the network
    cond = DIMSE_receiveCommand( m_assoc, m_cfg->getDIMSEBlockingMode(), m_cfg->getDIMSETimeout(),
                                 &presID, &message, NULL );
    // check if peer did release or abort, or if we have a valid message
    if( cond.good() )
    {
      DcmPresentationContextInfo presInfo;
      getPresentationContextInfo(m_assoc, presID, presInfo);
      cond = handleIncomingCommand(&message, presInfo);
    }
  }
  // Clean up on association termination.
  if( cond == DUL_PEERREQUESTEDRELEASE )
  {
    notifyReleaseRequest();
    ASC_acknowledgeRelease(m_assoc);
  }
  else if( cond == DUL_PEERABORTEDASSOCIATION )
  {
    notifyAbortRequest();
  }
  else
  {
    notifyDIMSEError(cond);
    ASC_abortAssociation( m_assoc );
  }

  // Drop and destroy the association.
  dropAndDestroyAssociation();

  // Output separator line.
  DCMNET_DEBUG( "+++++++++++++++++++++++++++++" );
}



// ----------------------------------------------------------------------------

OFCondition DcmMppsSCP::handleIncomingCommand(T_DIMSE_Message *incomingMsg,
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

// ----------------------------------------------------------------------------

// -- C-ECHO --

OFCondition DcmMppsSCP::handleECHORequest(T_DIMSE_C_EchoRQ &reqMessage,
                                      const T_ASC_PresentationContextID presID)
{
  OFCondition cond;
  OFString tempStr;

  // Dump debug information
  if (DCM_dcmnetLogger.isEnabledFor(OFLogger::DEBUG_LOG_LEVEL))
  {
    DCMNET_INFO("Received C-ECHO Request");
    DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, reqMessage, DIMSE_INCOMING, NULL, presID));
    DCMNET_INFO("Sending C-ECHO Response");
  } else {
    DCMNET_INFO("Received C-ECHO Request (MsgID " << reqMessage.MessageID << ")");
    DCMNET_INFO("Sending C-ECHO Response (" << DU_cechoStatusString(STATUS_Success) << ")");
  }

  // Send response message
  cond = DIMSE_sendEchoResponse( m_assoc, presID, &reqMessage, STATUS_Success, NULL );
  if( cond.bad() )
    DCMNET_ERROR("Cannot send C-ECHO Response: " << DimseCondition::dump(tempStr, cond));
  else
    DCMNET_DEBUG("C-ECHO Response successfully sent");

  return cond;
}

// ----------------------------------------------------------------------------

// -- N-CREATE --

OFCondition DcmMppsSCP::receiveCREATERequest(T_DIMSE_N_CreateRQ &reqMessage,
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

OFCondition DcmMppsSCP::sendCREATEResponse(T_ASC_PresentationContextID presID,
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

// ----------------------------------------------------------------------------

// -- N-SET --

OFCondition DcmMppsSCP::receiveSETRequest(T_DIMSE_N_SetRQ &reqMessage,
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

OFCondition DcmMppsSCP::sendSETResponse(T_ASC_PresentationContextID presID,
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

/* ************************************************************************* */
/*                            Various helpers                                */
/* ************************************************************************* */

// Sends a DIMSE command and possibly also instance data to the configured peer DICOM application
OFCondition DcmMppsSCP::sendDIMSEMessage(const T_ASC_PresentationContextID presID,
                                     T_DIMSE_Message *message,
                                     DcmDataset *dataObject,
                                     DcmDataset *statusDetail,
                                     DcmDataset **commandSet)
{
  if (m_assoc == NULL)
    return DIMSE_ILLEGALASSOCIATION;
  if (message == NULL)
    return DIMSE_NULLKEY;

  OFCondition cond;
  cond = DIMSE_sendMessageUsingMemoryData(m_assoc, presID, message, statusDetail, dataObject,
                                            NULL /*callback*/, NULL /*callbackData*/, commandSet);
  return cond;
}

// ----------------------------------------------------------------------------

// Receive DIMSE command (excluding dataset!) over the currently open association
OFCondition DcmMppsSCP::receiveDIMSECommand(T_ASC_PresentationContextID *presID,
                                        T_DIMSE_Message *message,
                                        DcmDataset **statusDetail,
                                        DcmDataset **commandSet,
                                        const Uint32 timeout)
{
  if (m_assoc == NULL)
    return DIMSE_ILLEGALASSOCIATION;

  OFCondition cond;
  if (timeout > 0)
  {
    /* call the corresponding DIMSE function to receive the command (use specified timeout) */
    cond = DIMSE_receiveCommand(m_assoc, DIMSE_NONBLOCKING, timeout, presID,
                                message, statusDetail, commandSet);
  } else {
    /* call the corresponding DIMSE function to receive the command (use default timeout) */
    cond = DIMSE_receiveCommand(m_assoc, m_cfg->getDIMSEBlockingMode(), m_cfg->getDIMSETimeout(), presID,
                                message, statusDetail, commandSet);
  }
  return cond;
}

// ----------------------------------------------------------------------------

// Receives one dataset (of instance data) via network from another DICOM application in memory
OFCondition DcmMppsSCP::receiveDIMSEDataset(T_ASC_PresentationContextID *presID,
                                        DcmDataset **dataObject)
{
  if (m_assoc == NULL)
    return DIMSE_ILLEGALASSOCIATION;

  OFCondition cond;
  cond = DIMSE_receiveDataSetInMemory(m_assoc, m_cfg->getDIMSEBlockingMode(), m_cfg->getDIMSETimeout(),
                                        presID, dataObject, NULL /*callback*/, NULL /*callbackData*/);

  if (cond.good())
  {
    DCMNET_DEBUG("Received dataset on presentation context " << OFstatic_cast(unsigned int, *presID));
  } else {
    OFString tempStr;
    DCMNET_ERROR("Unable to receive dataset on presentation context "
      << OFstatic_cast(unsigned int, *presID) << ": " << DimseCondition::dump(tempStr, cond));
  }
  return cond;
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::setMaxReceivePDULength(const Uint32 maxRecPDU)
{
  m_cfg->setMaxReceivePDULength(maxRecPDU);
}

// ----------------------------------------------------------------------------

OFCondition DcmMppsSCP::addPresentationContext(const OFString &abstractSyntax,
                                           const OFList<OFString> &xferSyntaxes,
                                           const T_ASC_SC_ROLE role,
                                           const OFString &profile)
{
  return m_cfg->addPresentationContext(abstractSyntax, xferSyntaxes, role, profile);
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::setPort(const Uint16 port)
{
  m_cfg->setPort(port);
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::setAETitle(const OFString &aetitle)
{
  m_cfg->setAETitle(aetitle);
}


// ----------------------------------------------------------------------------

void DcmMppsSCP::setRespondWithCalledAETitle(const OFBool useCalled)
{
  m_cfg->setRespondWithCalledAETitle(useCalled);
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::setConnectionBlockingMode(const DUL_BLOCKOPTIONS blockingMode)
{
  m_cfg->setConnectionBlockingMode(blockingMode);
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::setDIMSEBlockingMode(const T_DIMSE_BlockingMode blockingMode)
{
  m_cfg->setDIMSEBlockingMode(blockingMode);
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::setDIMSETimeout(const Uint32 dimseTimeout)
{
  m_cfg->setDIMSETimeout(dimseTimeout);
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::setACSETimeout(const Uint32 acseTimeout)
{
  m_cfg->setACSETimeout(acseTimeout);
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::setConnectionTimeout(const Uint32 timeout)
{
  m_cfg->setConnectionTimeout(timeout);
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::setVerbosePCMode(const OFBool mode)
{
  m_cfg->setVerbosePCMode(mode);
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::setHostLookupEnabled(const OFBool mode)
{
  m_cfg->setHostLookupEnabled(mode);
}

// ----------------------------------------------------------------------------

Uint32 DcmMppsSCP::getMaxReceivePDULength() const
{
  return m_cfg->getMaxReceivePDULength();
}

// ----------------------------------------------------------------------------

Uint16 DcmMppsSCP::getPort() const
{
  return m_cfg->getPort();
}

// ----------------------------------------------------------------------------

const OFString &DcmMppsSCP::getAETitle() const
{
  return m_cfg->getAETitle();
}

// ----------------------------------------------------------------------------

OFBool DcmMppsSCP::getRespondWithCalledAETitle() const
{
  return m_cfg->getRespondWithCalledAETitle();
}

// ----------------------------------------------------------------------------

DUL_BLOCKOPTIONS DcmMppsSCP::getConnectionBlockingMode() const
{
  return m_cfg->getConnectionBlockingMode();
}

// ----------------------------------------------------------------------------

T_DIMSE_BlockingMode DcmMppsSCP::getDIMSEBlockingMode() const
{
  return m_cfg->getDIMSEBlockingMode();
}

// ----------------------------------------------------------------------------

Uint32 DcmMppsSCP::getDIMSETimeout() const
{
  return m_cfg->getDIMSETimeout();
}

// ----------------------------------------------------------------------------

Uint32 DcmMppsSCP::getConnectionTimeout() const
{
  return m_cfg->getConnectionTimeout();
}

// ----------------------------------------------------------------------------

Uint32 DcmMppsSCP::getACSETimeout() const
{
  return m_cfg->getACSETimeout();
}

// ----------------------------------------------------------------------------

OFBool DcmMppsSCP::getVerbosePCMode() const
{
  return m_cfg->getVerbosePCMode();
}

// ----------------------------------------------------------------------------

OFBool DcmMppsSCP::getHostLookupEnabled() const
{
  return m_cfg->getHostLookupEnabled();
}

// ----------------------------------------------------------------------------

OFBool DcmMppsSCP::isConnected() const
{
  return (m_assoc != NULL) && (m_assoc->DULassociation != NULL);
}

// ----------------------------------------------------------------------------

OFString DcmMppsSCP::getPeerAETitle() const
{
  if (m_assoc == NULL)
    return "";
  return m_assoc->params->DULparams.callingAPTitle;
}

// ----------------------------------------------------------------------------

OFString DcmMppsSCP::getCalledAETitle() const
{
  if (m_assoc == NULL)
    return "";
  return m_assoc->params->DULparams.calledAPTitle;
}

// ----------------------------------------------------------------------------

Uint32 DcmMppsSCP::getPeerMaxPDULength() const
{
  if (m_assoc == NULL)
    return 0;
  return m_assoc->params->theirMaxPDUReceiveSize;
}

// ----------------------------------------------------------------------------

OFString DcmMppsSCP::getPeerIP() const
{
  if (m_assoc == NULL)
    return "";
  return m_assoc->params->DULparams.callingPresentationAddress;
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::dropAndDestroyAssociation()
{
  if (m_assoc)
  {
    notifyAssociationTermination();
    ASC_dropSCPAssociation( m_assoc );
    ASC_destroyAssociation( &m_assoc );
  }
}


/* ************************************************************************** */
/*                            Notify functions                                */
/* ************************************************************************** */


void DcmMppsSCP::notifyAssociationRequest(const T_ASC_Parameters &params,
                                      DcmSCPActionType & /* desiredAction */)
{
  // Dump some information if required
  DCMNET_INFO("Association Received " << params.DULparams.callingPresentationAddress << ": "
                                      << params.DULparams.callingAPTitle << " -> "
                                      << params.DULparams.calledAPTitle);

    // Dump more information if required
  OFString tempStr;
  if (m_cfg->getVerbosePCMode())
    DCMNET_INFO("Incoming Association Request:" << OFendl << ASC_dumpParameters(tempStr, m_assoc->params, ASC_ASSOC_RQ));
  else
    DCMNET_DEBUG("Incoming Association Request:" << OFendl << ASC_dumpParameters(tempStr, m_assoc->params, ASC_ASSOC_RQ));
}

// ----------------------------------------------------------------------------

OFBool DcmMppsSCP::checkCalledAETitleAccepted(const OFString& /*calledAETitle*/)
{
  return OFTrue;
}


// ----------------------------------------------------------------------------

OFBool DcmMppsSCP::checkCallingAETitleAccepted(const OFString& /*callingAETitle*/)
{
  return OFTrue;
}

// ----------------------------------------------------------------------------

OFBool DcmMppsSCP::checkCallingHostAccepted(const OFString& /*hostOrIP*/)
{
  return OFTrue;
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::notifyAssociationAcknowledge()
{
  DCMNET_DEBUG("DcmSCP: Association Acknowledged");
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::notifyReleaseRequest()
{
  DCMNET_INFO("Received Association Release Request");
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::notifyAbortRequest()
{
  DCMNET_INFO("Received Association Abort Request");
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::notifyAssociationTermination()
{
  DCMNET_DEBUG("DcmSCP: Association Terminated");
}

// ----------------------------------------------------------------------------

void DcmMppsSCP::notifyDIMSEError(const OFCondition &cond)
{
  OFString tempStr;
  DCMNET_DEBUG("DIMSE Error, detail (if available): " << DimseCondition::dump(tempStr, cond));
}

// ----------------------------------------------------------------------------

OFBool DcmMppsSCP::stopAfterCurrentAssociation()
{
  return OFFalse;
}

/* ************************************************************************* */
/*                         Static helper functions                           */
/* ************************************************************************* */

OFBool DcmMppsSCP::getPresentationContextInfo(const T_ASC_Association *assoc,
                                          const Uint8 presID,
                                          DcmPresentationContextInfo &presInfo)
{
  if (assoc != NULL)
  {
    DUL_PRESENTATIONCONTEXT *pc = findPresentationContextID(assoc->params->DULparams.acceptedPresentationContext, presID);
    if (pc != NULL)
    {
      presInfo.abstractSyntax = pc->abstractSyntax;
      presInfo.acceptedTransferSyntax = pc->acceptedTransferSyntax;
      presInfo.presentationContextID = pc->presentationContextID;
      presInfo.proposedSCRole = pc->proposedSCRole;
      presInfo.acceptedSCRole = pc->acceptedSCRole;
      return OFTrue;
    }
  }
  return OFFalse;
}


