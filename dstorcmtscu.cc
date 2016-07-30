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

#include "dstorcmtscu.h"
#include "dcmtk/dcmnet/diutil.h"

#include "dcmtk/ofstd/ofstd.h"

// DcmStorCmtSCU
//
DcmStorCmtSCU::DcmStorCmtSCU():
  OFThread(),
  m_assoc(NULL),
  m_net(NULL),
  m_params(NULL),
  m_presContexts(),
  m_maxReceivePDULength(ASC_DEFAULTMAXPDU),
  m_blockMode(DIMSE_BLOCKING),
  m_ourAETitle("ANY-SCU"),
  m_peer(),
  m_peerAETitle("ANY-SCP"),
  m_peerPort(104),
  m_dimseTimeout(0),
  m_acseTimeout(30)
{
    OFList<OFString> transferSyntaxes;
    transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
    transferSyntaxes.push_back(UID_BigEndianExplicitTransferSyntax);
    transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
    addPresentationContext(UID_StorageCommitmentPushModelSOPClass, transferSyntaxes);

    storageCommitCommand = NULL;
}

void DcmStorCmtSCU::freeNetwork()
{
  if ((m_assoc != NULL) || (m_net != NULL) || (m_params != NULL))
    DCMNET_DEBUG("Cleaning up internal association and network structures");
  /* destroy association parameters, i.e. free memory of T_ASC_Parameters.
     Usually this is done in ASC_destroyAssociation; however, if we already
     have association parameters but not yet an association (e.g. after calling
     initNetwork() and negotiateAssociation()), the latter approach may fail.
  */
  if (m_params)
  {
    ASC_destroyAssociationParameters(&m_params);
    m_params = NULL;
    // make sure destroyAssociation does not try to free params a second time
    // (happens in case we have already have an association structure)
    if (m_assoc)
      m_assoc->params = NULL;
  }
  // destroy the association, i.e. free memory of T_ASC_Association* structure.
  ASC_destroyAssociation(&m_assoc);
  // drop the network, i.e. free memory of T_ASC_Network* structure.
  ASC_dropNetwork(&m_net);

}

DcmStorCmtSCU::~DcmStorCmtSCU()
{
    if (storageCommitCommand != NULL )
    {
        if (storageCommitCommand->reqDataset != NULL) {
            delete storageCommitCommand->reqDataset;
        }
        delete storageCommitCommand;
        storageCommitCommand = NULL;
    }

  // abort association (if any) and destroy dcmnet data structures
  if (isConnected())
  {
    closeAssociation(DCMSCU_ABORT_ASSOCIATION); // also frees network
  } else {
    freeNetwork();
  }

}

OFCondition DcmStorCmtSCU::initNetwork()
{
  /* Return if SCU is already connected */
  if (isConnected())
    return NET_EC_AlreadyConnected;

  /* Be sure internal network structures are clean (delete old) */
  freeNetwork();

  OFString tempStr;
  /* initialize network, i.e. create an instance of T_ASC_Network*. */
  OFCondition cond = ASC_initializeNetwork(NET_REQUESTOR, 0, m_acseTimeout, &m_net);
  if (cond.bad())
  {
    DimseCondition::dump(tempStr, cond);
    DCMNET_ERROR(tempStr);
    return cond;
  }

  /* initialize association parameters, i.e. create an instance of T_ASC_Parameters*. */
  cond = ASC_createAssociationParameters(&m_params, m_maxReceivePDULength);
  if (cond.bad())
  {
    DCMNET_ERROR(DimseCondition::dump(tempStr, cond));
    return cond;
  }

  /* sets this application's title and the called application's title in the params */
  /* structure. The default values are "ANY-SCU" and "ANY-SCP". */
  ASC_setAPTitles(m_params, m_ourAETitle.c_str(), m_peerAETitle.c_str(), NULL);

  /* Figure out the presentation addresses and copy the */
  /* corresponding values into the association parameters.*/
  DIC_NODENAME localHost;
  DIC_NODENAME peerHost;
  gethostname(localHost, sizeof(localHost) - 1);
  /* Since the underlying dcmnet structures reserve only 64 bytes for peer
     as well as local host name, we check here for buffer overflow.
   */
  if ((m_peer.length() + 5 /* max 65535 */) + 1 /* for ":" */ > 63)
  {
    DCMNET_ERROR("Maximum length of peer host name '" << m_peer << "' is longer than maximum of 57 characters");
    return EC_IllegalCall; // TODO: need to find better error code
  }
  if (strlen(localHost) + 1 > 63)
  {
    DCMNET_ERROR("Maximum length of local host name '" << localHost << "' is longer than maximum of 62 characters");
    return EC_IllegalCall; // TODO: need to find better error code
  }
  sprintf(peerHost, "%s:%d", m_peer.c_str(), OFstatic_cast(int, m_peerPort));
  ASC_setPresentationAddresses(m_params, localHost, peerHost);

  /* Add presentation contexts */

  // Adapt presentation context ID to existing presentation contexts.
  // It's important that presentation context IDs are numerated 1,3,5,7...!
  Uint32 nextFreePresID = 257;
  Uint32 numContexts = ASC_countPresentationContexts(m_params);
  if (numContexts <= 127)
  {
    // Need Uint16 to avoid overflow in currPresID (unsigned char)
    nextFreePresID = 2 * numContexts + 1; /* add 1 to point to the next free ID*/
  }
  // Print warning if number of overall presentation contexts exceeds 128
  if ((numContexts + m_presContexts.size()) > 128)
  {
    DCMNET_WARN("Number of presentation contexts exceeds 128 (" << numContexts + m_presContexts.size()
      << "). Some contexts will not be negotiated");
  }
  else
  {
    DCMNET_TRACE("Configured " << numContexts << " presentation contexts from config file");
    if (m_presContexts.size() > 0)
      DCMNET_TRACE("Adding another " << m_presContexts.size() << " presentation contexts configured for SCU");
  }


  // Add presentation contexts not originating from config file
  OFListIterator(DcmSCUPresContext) contIt = m_presContexts.begin();
  OFListConstIterator(DcmSCUPresContext) endOfContList = m_presContexts.end();
  while ((contIt != endOfContList) && (nextFreePresID <= 255))
  {
    const Uint16 numTransferSyntaxes = OFstatic_cast(Uint16, (*contIt).transferSyntaxes.size());
    const char** transferSyntaxes = new const char*[numTransferSyntaxes];

    // Iterate over transfer syntaxes within one presentation context
    OFListIterator(OFString) syntaxIt = (*contIt).transferSyntaxes.begin();
    OFListIterator(OFString) endOfSyntaxList = (*contIt).transferSyntaxes.end();
    Uint16 sNum = 0;
    // copy all transfer syntaxes to array
    while (syntaxIt != endOfSyntaxList)
    {
      transferSyntaxes[sNum] = (*syntaxIt).c_str();
      ++syntaxIt;
      ++sNum;
    }

    // add the presentation context
    cond = ASC_addPresentationContext(m_params, OFstatic_cast(Uint8, nextFreePresID),
      (*contIt).abstractSyntaxName.c_str(), transferSyntaxes, numTransferSyntaxes,(*contIt).roleSelect);
    // if adding was successful, prepare presentation context ID for next addition
    delete[] transferSyntaxes;
    transferSyntaxes = NULL;
    if (cond.bad())
      return cond;
    contIt++;
    // goto next free number, only odd presentation context IDs permitted
    nextFreePresID += 2;
  }
  numContexts = ASC_countPresentationContexts(m_params);
  if (numContexts == 0)
  {
    DCMNET_ERROR("Cannot initialize network: No presentation contexts defined");
    return NET_EC_NoPresentationContextsDefined;
  }
  DCMNET_DEBUG("Configured a total of " << numContexts << " presentation contexts for SCU");

  return cond;
}


OFCondition DcmStorCmtSCU::negotiateAssociation()
{
  /* Return error if SCU is already connected */
  if (isConnected())
    return NET_EC_AlreadyConnected;

  /* dump presentation contexts if required */
  OFString tempStr;
  if (m_verbosePCMode)
    DCMNET_INFO("Request Parameters:" << OFendl << ASC_dumpParameters(tempStr, m_params, ASC_ASSOC_RQ));
  else
    DCMNET_DEBUG("Request Parameters:" << OFendl << ASC_dumpParameters(tempStr, m_params, ASC_ASSOC_RQ));

  /* create association, i.e. try to establish a network connection to another */
  /* DICOM application. This call creates an instance of T_ASC_Association*. */
  DCMNET_INFO("Requesting Association");
  OFCondition cond = ASC_requestAssociation(m_net, m_params, &m_assoc);
  if (cond.bad())
  {
    if (cond == DUL_ASSOCIATIONREJECTED)
    {
      T_ASC_RejectParameters rej;
      ASC_getRejectParameters(m_params, &rej);
      DCMNET_DEBUG("Association Rejected:" << OFendl << ASC_printRejectParameters(tempStr, &rej));
      return cond;
    }
    else
    {
      DCMNET_DEBUG("Association Request Failed: " << DimseCondition::dump(tempStr, cond));
      return cond;
    }
  }
  /* dump the presentation contexts which have been accepted/refused */
  if (m_verbosePCMode)
    DCMNET_INFO("Association Parameters Negotiated:" << OFendl << ASC_dumpParameters(tempStr, m_params, ASC_ASSOC_AC));
  else
    DCMNET_DEBUG("Association Parameters Negotiated:" << OFendl << ASC_dumpParameters(tempStr, m_params, ASC_ASSOC_AC));

  /* count the presentation contexts which have been accepted by the SCP */
  /* If there are none, finish the execution */
  if (ASC_countAcceptedPresentationContexts(m_params) == 0)
  {
    DCMNET_ERROR("No Acceptable Presentation Contexts");
    return NET_EC_NoAcceptablePresentationContexts;
  }

  /* dump general information concerning the establishment of the network connection if required */
  DCMNET_INFO("Association Accepted (Max Send PDV: " << OFstatic_cast(unsigned long, m_assoc->sendPDVLength) << ")");
  return EC_Normal;
}


OFCondition DcmStorCmtSCU::addPresentationContext(const OFString &abstractSyntax,
                                           const OFList<OFString> &xferSyntaxes,
                                           const T_ASC_SC_ROLE role)

{

  DcmSCUPresContext presContext;
  presContext.abstractSyntaxName = abstractSyntax;
  OFListConstIterator(OFString) it = xferSyntaxes.begin();
  OFListConstIterator(OFString) endOfList = xferSyntaxes.end();
  while (it != endOfList)
  {
    presContext.transferSyntaxes.push_back(*it);
    it++;
  }
  presContext.roleSelect = role;
  m_presContexts.push_back(presContext);
  return EC_Normal;
}

void DcmStorCmtSCU::clearPresentationContexts()
{
  m_presContexts.clear();
}

// Returns usable presentation context ID for a given abstract syntax UID and
// transfer syntax UID. 0 if none matches.
T_ASC_PresentationContextID DcmStorCmtSCU::findPresentationContextID(const OFString &abstractSyntax,
                                                              const OFString &transferSyntax)
{
  if (!isConnected())
    return 0;

  DUL_PRESENTATIONCONTEXT *pc;
  LST_HEAD **l;
  OFBool found = OFFalse;

  if (abstractSyntax.empty()) return 0;

  /* first of all we look for a presentation context
   * matching both abstract and transfer syntax
   */
  l = &m_assoc->params->DULparams.acceptedPresentationContext;
  pc = (DUL_PRESENTATIONCONTEXT*) LST_Head(l);
  (void)LST_Position(l, (LST_NODE*)pc);
  while (pc && !found)
  {
    found = (strcmp(pc->abstractSyntax, abstractSyntax.c_str()) == 0);
    found &= (pc->result == ASC_P_ACCEPTANCE);
    if (!transferSyntax.empty())  // ignore transfer syntax if not specified
      found &= (strcmp(pc->acceptedTransferSyntax, transferSyntax.c_str()) == 0);
    if (!found) pc = (DUL_PRESENTATIONCONTEXT*) LST_Next(l);
  }
  if (found)
    return pc->presentationContextID;

  return 0;   /* not found */
}

void DcmStorCmtSCU::findPresentationContext(const T_ASC_PresentationContextID presID,
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


Uint16 DcmStorCmtSCU::nextMessageID()
{
  if (!isConnected())
    return 0;
  else
    return m_assoc->nextMsgID++;
}

void DcmStorCmtSCU::closeAssociation(const DcmCloseAssociationType closeType)
{
  if (!isConnected())
  {
    DCMNET_WARN("Closing of association requested but no association active (ignored)");
    return;
  }

  OFCondition cond;
  OFString tempStr;

  /* tear down association, i.e. terminate network connection to SCP */
  switch (closeType)
  {
    case DCMSCU_RELEASE_ASSOCIATION:
      /* release association */
      DCMNET_INFO("Releasing Association");
      cond = ASC_releaseAssociation(m_assoc);
      if (cond.bad())
      {
        DCMNET_ERROR("Association Release Failed: " << DimseCondition::dump(tempStr, cond));
        return; // TODO: do we really need this?
      }
      break;
   case DCMSCU_ABORT_ASSOCIATION:
      /* abort association */
      DCMNET_INFO("Aborting Association");
      cond = ASC_abortAssociation(m_assoc);
      if (cond.bad())
      {
        DCMNET_ERROR("Association Abort Failed: " << DimseCondition::dump(tempStr, cond));
      }
      break;
    case DCMSCU_PEER_REQUESTED_RELEASE:
      /* peer requested release */
      DCMNET_ERROR("Protocol Error: Peer requested release (Aborting)");
      DCMNET_INFO("Aborting Association");
      cond = ASC_abortAssociation(m_assoc);
      if (cond.bad())
      {
        DCMNET_ERROR("Association Abort Failed: " << DimseCondition::dump(tempStr, cond));
      }
      break;
    case DCMSCU_PEER_ABORTED_ASSOCIATION:
      /* peer aborted association */
      DCMNET_INFO("Peer Aborted Association");
      break;
  }

  // destroy and free memory of internal association and network structures
  freeNetwork();
}


OFCondition DcmStorCmtSCU::releaseAssociation()
{
    OFCondition status = DIMSE_ILLEGALASSOCIATION;
    // check whether there is an active association
    if (isConnected())
    {
        closeAssociation(DCMSCU_RELEASE_ASSOCIATION);
        status = EC_Normal;
    }
    return status;
}


OFCondition DcmStorCmtSCU::abortAssociation()
{
    OFCondition status = DIMSE_ILLEGALASSOCIATION;
    // check whether there is an active association
    if (isConnected())
    {
        closeAssociation(DCMSCU_ABORT_ASSOCIATION);
        status = EC_Normal;
    }
    return status;
}

void DcmStorCmtSCU::setStorageCommitCommand( DcmStorageCommitmentCommand *command )
{
    storageCommitCommand = new DcmStorageCommitmentCommand();
    storageCommitCommand->localAETitle = command->localAETitle;
    storageCommitCommand->remoteAETitle = command->remoteAETitle;
    storageCommitCommand->remoteHostName = command->remoteIP;
    storageCommitCommand->remoteIP = command->remoteIP;
    storageCommitCommand->remotePort = command->remotePort;
    storageCommitCommand->reqDataset = (DcmDataset *)command->reqDataset->clone();

    setAETitle(storageCommitCommand->localAETitle);
    setPeerHostName(storageCommitCommand->remoteIP);
    setPeerAETitle(storageCommitCommand->remoteAETitle);
    setPeerPort(storageCommitCommand->remotePort);

}

/* ************************************************************************* */
/*                         N-EVENT REPORT functionality                      */
/* ************************************************************************* */

// Sends N-EVENT-REPORT request and receives N-EVENT-REPORT response
OFCondition DcmStorCmtSCU::sendEVENTREPORTRequest(const T_ASC_PresentationContextID presID,
                                           const OFString &sopInstanceUID,
                                           const Uint16 eventTypeID,
                                           DcmDataset *reqDataset,
                                           Uint16 &rspStatusCode)
{
  // Do some basic validity checks
  if (!isConnected())
    return DIMSE_ILLEGALASSOCIATION;
  if (sopInstanceUID.empty() || (reqDataset == NULL))
    return DIMSE_NULLKEY;

  // Prepare DIMSE data structures for issuing request
  OFCondition cond;
  OFString tempStr;
  T_ASC_PresentationContextID pcid = presID;
  T_DIMSE_Message request;
  // Make sure everything is zeroed (especially options)
  bzero((char*)&request, sizeof(request));

  T_DIMSE_N_EventReportRQ &eventReportReq = request.msg.NEventReportRQ;
  DcmDataset *statusDetail = NULL;

  request.CommandField = DIMSE_N_EVENT_REPORT_RQ;

  // Generate a new message ID
  eventReportReq.MessageID = nextMessageID();
  eventReportReq.DataSetType = DIMSE_DATASET_PRESENT;
  eventReportReq.EventTypeID = eventTypeID;

  // Determine SOP Class from presentation context
  OFString abstractSyntax, transferSyntax;
  findPresentationContext(pcid, abstractSyntax, transferSyntax);
  if (abstractSyntax.empty() || transferSyntax.empty())
    return DIMSE_NOVALIDPRESENTATIONCONTEXTID;
  OFStandard::strlcpy(eventReportReq.AffectedSOPClassUID, abstractSyntax.c_str(), sizeof(eventReportReq.AffectedSOPClassUID));
  OFStandard::strlcpy(eventReportReq.AffectedSOPInstanceUID, sopInstanceUID.c_str(), sizeof(eventReportReq.AffectedSOPInstanceUID));

  // Send request
  if (DCM_dcmnetLogger.isEnabledFor(OFLogger::DEBUG_LOG_LEVEL))
  {
    DCMNET_INFO("Sending N-EVENT-REPORT Request");
    // Output dataset only if trace level is enabled
    if (DCM_dcmnetLogger.isEnabledFor(OFLogger::TRACE_LOG_LEVEL))
      DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, request, DIMSE_OUTGOING, reqDataset, pcid));
    else
      DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, request, DIMSE_OUTGOING, NULL, pcid));
  } else {
    DCMNET_INFO("Sending N-EVENT-REPORT Request (MsgID " << eventReportReq.MessageID << ")");
  }
  cond = sendDIMSEMessage(pcid, &request, reqDataset);
  if (cond.bad())
  {
    DCMNET_ERROR("Failed sending N-EVENT-REPORT request: " << DimseCondition::dump(tempStr, cond));
    return cond;
  }
  // Receive response
  T_DIMSE_Message response;
  // Make sure everything is zeroed (especially options)
  bzero((char*)&response, sizeof(response));

  cond = receiveDIMSECommand(&pcid, &response, &statusDetail, NULL /* commandSet */);
  if (cond.bad())
  {
    DCMNET_ERROR("Failed receiving DIMSE response: " << DimseCondition::dump(tempStr, cond));
    return cond;
  }

  // Check command set
  if (response.CommandField == DIMSE_N_EVENT_REPORT_RSP)
  {
    if (DCM_dcmnetLogger.isEnabledFor(OFLogger::DEBUG_LOG_LEVEL))
    {
      DCMNET_INFO("Received N-EVENT-REPORT Response");
      DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, response, DIMSE_INCOMING, NULL, pcid));
    } else {
      DCMNET_INFO("Received N-EVENT-REPORT Response (" << DU_neventReportStatusString(response.msg.NEventReportRSP.DimseStatus) << ")");
    }
  } else {
    DCMNET_ERROR("Expected N-EVENT-REPORT response but received DIMSE command 0x"
      << STD_NAMESPACE hex << STD_NAMESPACE setfill('0') << STD_NAMESPACE setw(4)
      << OFstatic_cast(unsigned int, response.CommandField));
    DCMNET_DEBUG(DIMSE_dumpMessage(tempStr, response, DIMSE_INCOMING, NULL, pcid));
    delete statusDetail;
    return DIMSE_BADCOMMANDTYPE;
  }
  if (statusDetail != NULL)
  {
    DCMNET_DEBUG("Response has status detail:" << OFendl << DcmObject::PrintHelper(*statusDetail));
    delete statusDetail;
  }

  // Set return value
  T_DIMSE_N_EventReportRSP &eventReportRsp = response.msg.NEventReportRSP;
  rspStatusCode = eventReportRsp.DimseStatus;

  // Check whether there is a dataset to be received
  if (eventReportRsp.DataSetType == DIMSE_DATASET_PRESENT)
  {
    // this should never happen
    DcmDataset *tempDataset = NULL;
    T_ASC_PresentationContextID tempID;
    cond = receiveDIMSEDataset(&tempID, &tempDataset);
    if (cond.good())
    {
      DCMNET_WARN("Received unexpected dataset after N-EVENT-REPORT response, ignoring");
      delete tempDataset;
    } else {
      DCMNET_ERROR("Failed receiving unexpected dataset after N-EVENT-REPORT response: "
        << DimseCondition::dump(tempStr, cond));
      return DIMSE_BADDATA;
    }
  }
  return cond;
}

/* ************************************************************************* */
/*                            Various helpers                                */
/* ************************************************************************* */

// Sends a DIMSE command and possibly also instance data to the configured peer DICOM application
OFCondition DcmStorCmtSCU::sendDIMSEMessage(const T_ASC_PresentationContextID presID,
                                     T_DIMSE_Message *msg,
                                     DcmDataset *dataObject,
                                     DcmDataset **commandSet)
{
  if (!isConnected())
    return DIMSE_ILLEGALASSOCIATION;
  if (msg == NULL)
    return DIMSE_NULLKEY;

  OFCondition cond;
  cond = DIMSE_sendMessageUsingMemoryData(m_assoc, presID, msg, NULL /*statusDetail*/, dataObject,
                                            NULL /*callback*/, NULL /*callbackData*/, commandSet);
  return cond;
}

// Receive DIMSE command (excluding dataset!) over the currently open association
OFCondition DcmStorCmtSCU::receiveDIMSECommand(T_ASC_PresentationContextID *presID,
                                        T_DIMSE_Message *msg,
                                        DcmDataset **statusDetail,
                                        DcmDataset **commandSet,
                                        const Uint32 timeout)
{
  if (!isConnected())
    return DIMSE_ILLEGALASSOCIATION;

  OFCondition cond;
  if (timeout > 0)
  {
    /* call the corresponding DIMSE function to receive the command (use specified timeout)*/
    cond = DIMSE_receiveCommand(m_assoc, DIMSE_NONBLOCKING, timeout, presID,
                                msg, statusDetail, commandSet);
  } else {
    /* call the corresponding DIMSE function to receive the command (use default timeout) */
    cond = DIMSE_receiveCommand(m_assoc, m_blockMode, m_dimseTimeout, presID,
                                msg, statusDetail, commandSet);
  }
  return cond;
}

// Receives one dataset (of instance data) via network from another DICOM application
OFCondition DcmStorCmtSCU::receiveDIMSEDataset(T_ASC_PresentationContextID *presID,
                                        DcmDataset **dataObject)
{
  if (!isConnected())
    return DIMSE_ILLEGALASSOCIATION;

  OFCondition cond;
  cond = DIMSE_receiveDataSetInMemory(m_assoc, m_blockMode, m_dimseTimeout, presID, dataObject,
                                        NULL /*callback*/, NULL /*callbackData*/);
  if (cond.good())
  {
    DCMNET_DEBUG("Received dataset on presentation context " << OFstatic_cast(unsigned int, *presID));
  }
  else
  {
    OFString tempStr;
    DCMNET_ERROR("Unable to receive dataset on presentation context "
      << OFstatic_cast(unsigned int, *presID) << ": " << DimseCondition::dump(tempStr, cond));
  }
  return cond;
}


void DcmStorCmtSCU::setMaxReceivePDULength(const Uint32 maxRecPDU)
{
  m_maxReceivePDULength = maxRecPDU;
}


void DcmStorCmtSCU::setDIMSEBlockingMode(const T_DIMSE_BlockingMode blockingMode)
{
  m_blockMode = blockingMode;
}


void DcmStorCmtSCU::setAETitle(const OFString &myAETtitle)
{
  m_ourAETitle = myAETtitle;
}


void DcmStorCmtSCU::setPeerHostName(const OFString &peerHostName)
{
  m_peer = peerHostName;
}


void DcmStorCmtSCU::setPeerAETitle(const OFString &peerAETitle)
{
  m_peerAETitle = peerAETitle;
}

void DcmStorCmtSCU::setPeerPort(const Uint16 peerPort)
{
  m_peerPort = peerPort;
}
void DcmStorCmtSCU::setVerbosePCMode(const OFBool mode)
{
  m_verbosePCMode = mode;
}

/* Get methods */

OFBool DcmStorCmtSCU::isConnected() const
{
  return (m_assoc != NULL) && (m_assoc->DULassociation != NULL);
}

OFBool DcmStorCmtSCU::getVerbosePCMode() const
{
  return m_verbosePCMode;
}

int DcmStorCmtSCU::start()
{
    return OFThread::start();
}

int DcmStorCmtSCU::join()
{
    return OFThread::join();
}

void DcmStorCmtSCU::run()
{
    OFCondition cond = EC_Normal;

    DCMNET_DEBUG("SCU: initNetwork START");
    cond = initNetwork();
    if (cond.bad()) {
        OFString tempStr;
        DCMNET_ERROR(DimseCondition::dump(tempStr, cond));
        return;
    }
    DCMNET_DEBUG("SCU: initNetwork END");

    DCMNET_DEBUG("SCU: negotiateAssociation START");
    cond = negotiateAssociation();
    if (cond.bad()) {
        OFString tempStr;
        DCMNET_ERROR(DimseCondition::dump(tempStr, cond));
        return;
    }
    DCMNET_DEBUG("SCU: negotiateAssociation END");

    T_ASC_PresentationContextID presID = 0;
    if (presID == 0)
        presID = findPresentationContextID(UID_StorageCommitmentPushModelSOPClass, UID_LittleEndianExplicitTransferSyntax);
    if (presID == 0)
    {
        DCMNET_ERROR("No presentation context found for sending N-EVENT-REPORT with SOP Class / Transfer Syntax");
        return;
    }


    OFString sopInstanceUID = UID_StorageCommitmentPushModelSOPInstance;
    Uint16 eventTypeID = 1;
    Uint16 rspStatusCode = 0; 
    cond = sendEVENTREPORTRequest(presID,sopInstanceUID,eventTypeID,storageCommitCommand->reqDataset,rspStatusCode);
    if (cond.bad()) {
        OFString tempStr;
        DCMNET_ERROR(DimseCondition::dump(tempStr, cond));
        return;
    }

    DCMNET_DEBUG("SCU: closeAssociation START");
    closeAssociation(DCMSCU_RELEASE_ASSOCIATION);
    DCMNET_DEBUG("SCU: closeAssociation END");

    return;
}


