
#include "storcmtscu.h"

#include "dcmtk/ofstd/ofstd.h"
#include <dcmtk/dcmnet/assoc.h>

#define OFFIS_CONSOLE_APPLICATION "storcmtscu"

#define APPLICATIONTITLE "STORCMTSCU"     /* our application entity title */

static OFLogger logger = OFLog::getLogger("dcmtk.apps." OFFIS_CONSOLE_APPLICATION);

#define SHORTCOL 4
#define LONGCOL 21

int main(int argc, char *argv[])
{
    OFConsoleApplication app(OFFIS_CONSOLE_APPLICATION, "DICOM Storage Commitment (N-EVENT-REPORT) SCU");
    OFCommandLine cmd;

    /* evaluate command line */
    prepareCmdLineArgs(argc, argv, OFFIS_CONSOLE_APPLICATION);
    if (app.parseCommandLine(cmd, argc, argv, OFCommandLine::PF_ExpandWildcards))
    {
        /* print help text and exit */
        if (cmd.getArgCount() == 0)
            app.printUsage();

        OFLog::configureFromCommandLine(cmd, app);

    }

    OFString config = "storcmtscu.cfg";

    DcmStorCmtSCU *storcmtscu = new DcmStorCmtSCU();

    delete storcmtscu;
}

// DcmStorCmtSCU
//
DcmStorCmtSCU::DcmStorCmtSCU():
  m_assoc(NULL),
  m_net(NULL),
  m_params(NULL),
  m_assocConfigFilename(),
  m_assocConfigProfile(),
  m_presContexts(),
  m_openDIMSERequest(NULL),
  m_maxReceivePDULength(ASC_DEFAULTMAXPDU),
  m_blockMode(DIMSE_BLOCKING),
  m_ourAETitle("ANY-SCU"),
  m_peer(),
  m_peerAETitle("ANY-SCP"),
  m_peerPort(104),
  m_dimseTimeout(0),
  m_acseTimeout(30),
  m_verbosePCMode(OFFalse)
{

}

DcmStorCmtSCU::~DcmStorCmtSCU()
{
  // abort association (if any) and destroy dcmnet data structures
  if (isConnected())
  {
    closeAssociation(DCMSCU_ABORT_ASSOCIATION);
  } else {
    if ((m_assoc != NULL) || (m_net != NULL))
      OFLOG_DEBUG(logger,"Cleaning up internal association and network structures");
    ASC_destroyAssociation(&m_assoc);
    ASC_dropNetwork(&m_net);
  }
  // free memory allocated by this class
  delete m_openDIMSERequest;


}

OFCondition DcmStorCmtSCU::initNetwork()
{
  // TODO: do we need to check whether the network is already initialized?
  OFString tempStr;
  /* initialize network, i.e. create an instance of T_ASC_Network*. */
  OFCondition cond = ASC_initializeNetwork(NET_REQUESTOR, 0, m_acseTimeout, &m_net);
  if (cond.bad())
  {
    DimseCondition::dump(tempStr, cond);
    OFLOG_ERROR(logger,tempStr);
    return cond;
  }

  /* initialize asscociation parameters, i.e. create an instance of T_ASC_Parameters*. */
  cond = ASC_createAssociationParameters(&m_params, m_maxReceivePDULength);
  if (cond.bad())
  {
    OFLOG_ERROR(logger,DimseCondition::dump(tempStr, cond));
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
    OFLOG_ERROR(logger,"Maximum length of peer host name '" << m_peer << "' is longer than maximum of 57 characters");
    return EC_IllegalCall; // TODO: need to find better error code
  }
  if (strlen(localHost) + 1 > 63)
  {
    OFLOG_ERROR(logger,"Maximum length of local host name '" << localHost << " is longer than maximum of 62 characters");
    return EC_IllegalCall; // TODO: need to find better error code
  }
  sprintf(peerHost, "%s:%d", m_peer.c_str(), OFstatic_cast(int, m_peerPort));
  ASC_setPresentationAddresses(m_params, localHost, peerHost);

  /* Add presentation contexts */

  // First, import from config file, if specified
  OFCondition result;
  if (m_assocConfigFilename.length() != 0)
  {
    DcmAssociationConfiguration assocConfig;
    result = DcmAssociationConfigurationFile::initialize(assocConfig, m_assocConfigFilename.c_str());
    if (result.bad())
    {
      OFLOG_WARN(logger,"Unable to parse association configuration file " << m_assocConfigFilename
        << " (ignored): " << result.text());
      return result;
    }
    else
    {
      /* perform name mangling for config file key */
      OFString profileName;
      const unsigned char *c = OFreinterpret_cast(const unsigned char *, m_assocConfigProfile.c_str());
      while (*c)
      {
        if (! isspace(*c)) profileName += OFstatic_cast(char, toupper(*c));
        ++c;
      }

      result = assocConfig.setAssociationParameters(profileName.c_str(), *m_params);
      if (result.bad())
      {
        OFLOG_WARN(logger,"Unable to apply association configuration file" << m_assocConfigFilename
          <<" (ignored): " << result.text());
        return result;
      }
    }
  }

  // Adapt presentation context ID to existing presentation contexts
  // It's important that presentation context ids are numerated 1,3,5,7...!
  Uint32 nextFreePresID = 257;
  Uint32 numContexts = ASC_countPresentationContexts(m_params);
  if (numContexts <= 127)
  {
    // Need Uint16 to avoid overflow in currPresID (unsigned char)
    nextFreePresID = 2 * numContexts + 1; /* add 1 to point to the next free ID*/
  }
  // Print warning if number of overall presenation contexts exceeds 128
  if ((numContexts + m_presContexts.size()) > 128)
  {
    OFLOG_WARN(logger,"Number of presentation contexts exceeds 128 (" << numContexts + m_presContexts.size()
      << "). Some contexts will not be negotiated");
  }
  else
  {
    OFLOG_TRACE(logger,"Configured " << numContexts << " presentation contexts from config file");
    if (m_presContexts.size() > 0)
        OFLOG_TRACE(logger,"Adding another " << m_presContexts.size() << " presentation contexts configured for SCU");
  }

  // Add presentation contexts not originating from config file
  OFListIterator(DcmSCUPresContext) contIt = m_presContexts.begin();
  OFListConstIterator(DcmSCUPresContext) endOfContList = m_presContexts.end();
  while ((contIt != endOfContList) && (nextFreePresID <= 255))
  {
    const Uint16 numTransferSyntaxes = (*contIt).transferSyntaxes.size();
    const char** transferSyntaxes = new const char*[numTransferSyntaxes];

    // Iterate over transfer syntaxes within one presentation context
    OFListIterator(OFString) syntaxIt = (*contIt).transferSyntaxes.begin();
    OFListIterator(OFString) endOfSyntaxList = (*contIt).transferSyntaxes.end();
    Uint16 sNum = 0;
    // copy all transfersyntaxes to array
    while (syntaxIt != endOfSyntaxList)
    {
      transferSyntaxes[sNum] = (*syntaxIt).c_str();
      ++syntaxIt;
      ++sNum;
    }

    // add the presentation context
    cond = ASC_addPresentationContext(m_params, OFstatic_cast(Uint8, nextFreePresID),
      (*contIt).abstractSyntaxName.c_str(), transferSyntaxes, numTransferSyntaxes);
    // if adding was successfull, prepare pres. context ID for next addition
    delete[] transferSyntaxes;
    transferSyntaxes = NULL;
    if (cond.bad())
      return cond;
    contIt++;
    // goto next free nr, only odd presentation context numbers permitted
    nextFreePresID += 2;
  }

  numContexts = ASC_countPresentationContexts(m_params);
  if (numContexts == 0)
  {
    OFLOG_ERROR(logger,"Cannot initialize network: No presentation contexts defined");
    return DIMSE_NOVALIDPRESENTATIONCONTEXTID;
  }
  OFLOG_DEBUG(logger,"Configured a total of " << numContexts << " presentation contexts for SCU");

  return cond;
}

OFCondition DcmStorCmtSCU::negotiateAssociation()
{
  /* dump presentation contexts if required */
  OFString tempStr;
  if (m_verbosePCMode)
    OFLOG_INFO(logger,"Request Parameters:" << OFendl << ASC_dumpParameters(tempStr, m_params, ASC_ASSOC_RQ));
  else
    OFLOG_DEBUG(logger,"Request Parameters:" << OFendl << ASC_dumpParameters(tempStr, m_params, ASC_ASSOC_RQ));

  /* create association, i.e. try to establish a network connection to another */
  /* DICOM application. This call creates an instance of T_ASC_Association*. */
  OFLOG_INFO(logger,"Requesting Association");
  OFCondition cond = ASC_requestAssociation(m_net, m_params, &m_assoc);
  if (cond.bad())
  {
    if (cond == DUL_ASSOCIATIONREJECTED)
    {
      T_ASC_RejectParameters rej;

      ASC_getRejectParameters(m_params, &rej);
      OFLOG_DEBUG(logger,"Association Rejected:" << OFendl << ASC_printRejectParameters(tempStr, &rej));
      return cond;
    }
    else
    {
      OFLOG_DEBUG(logger,"Association Request Failed: " << DimseCondition::dump(tempStr, cond));
      return cond;
    }
  }

  /* dump the presentation contexts which have been accepted/refused */
  if (m_verbosePCMode)
    OFLOG_INFO(logger,"Association Parameters Negotiated:" << OFendl << ASC_dumpParameters(tempStr, m_params, ASC_ASSOC_AC));
  else
    OFLOG_DEBUG(logger,"Association Parameters Negotiated:" << OFendl << ASC_dumpParameters(tempStr, m_params, ASC_ASSOC_AC));

  /* count the presentation contexts which have been accepted by the SCP */
  /* If there are none, finish the execution */
  if (ASC_countAcceptedPresentationContexts(m_params) == 0)
  {
    OFLOG_ERROR(logger,"No Acceptable Presentation Contexts");
    return DIMSE_NOVALIDPRESENTATIONCONTEXTID;
  }

  /* dump general information concerning the establishment of the network connection if required */
  OFLOG_INFO(logger,"Association Accepted (Max Send PDV: " << OFstatic_cast(unsigned long, m_assoc->sendPDVLength) << ")");
  return EC_Normal;
}

// Sends C-ECHO request to another DICOM application
OFCondition DcmStorCmtSCU::sendECHORequest(const T_ASC_PresentationContextID presID)
{
  if (m_assoc == NULL)
    return DIMSE_ILLEGALASSOCIATION;

  OFCondition cond;
  T_ASC_PresentationContextID pcid = presID;

  /* If necessary, find appropriate presentation context */
  if (pcid == 0)
    pcid = findPresentationContextID(UID_VerificationSOPClass, UID_LittleEndianExplicitTransferSyntax);
  if (pcid == 0)
    pcid = findPresentationContextID(UID_VerificationSOPClass, UID_BigEndianExplicitTransferSyntax);
  if (pcid == 0)
    pcid = findPresentationContextID(UID_VerificationSOPClass, UID_LittleEndianImplicitTransferSyntax);
  if (pcid == 0)
  {
    OFLOG_ERROR(logger,"No presentation context found for sending C-ECHO with SOP Class / Transfer Syntax: "
      << dcmFindNameOfUID(UID_VerificationSOPClass, "") << "/"
      << DcmXfer(UID_LittleEndianImplicitTransferSyntax).getXferName());
    return DIMSE_NOVALIDPRESENTATIONCONTEXTID;
  }

  /* Now, assemble dimse message */
  Uint16 status;
  cond = DIMSE_echoUser(m_assoc, nextMessageID(), m_blockMode, m_dimseTimeout, &status, NULL);
  if (cond.bad())
  {
    OFString tempStr;
    OFLOG_ERROR(logger,"Failed sending C-ECHO request or receiving response: " << DimseCondition::dump(tempStr, cond));
    return cond;
  }
  else
  {
    if (status == STATUS_Success)
      OFLOG_DEBUG(logger,"Successfully sent C-ECHO request");
    else
    {
      OFLOG_ERROR(logger,"C-ECHO failed with status code: 0x"
        << STD_NAMESPACE hex << STD_NAMESPACE setfill('0') << STD_NAMESPACE setw(4) << status);
      return makeOFCondition(OFM_dcmnet, 22, OF_error, "SCP returned non-success status in C-ECHO response");
    }
  }
  return EC_Normal;
}

// Sends N-EVENT-REPORT request to another DICOM application
OFCondition DcmStorCmtSCU::sendEVENTREPORTRequest(const T_ASC_PresentationContextID presID,
                                        const OFString &sopInstanceUID,
                                        const Uint16 actionTypeID,
                                        DcmDataset *reqDataset,
                                        Uint16 &rspStatusCode)
{
  if (m_assoc == NULL)
    return DIMSE_ILLEGALASSOCIATION;

  OFCondition cond;
  T_ASC_PresentationContextID pcid = presID;

  /* If necessary, find appropriate presentation context */
  if (pcid == 0)
    pcid = findPresentationContextID(UID_StorageCommitmentPushModelSOPClass, UID_LittleEndianExplicitTransferSyntax);
  if (pcid == 0)
    pcid = findPresentationContextID(UID_StorageCommitmentPushModelSOPClass, UID_BigEndianExplicitTransferSyntax);
  if (pcid == 0)
    pcid = findPresentationContextID(UID_StorageCommitmentPushModelSOPClass, UID_LittleEndianImplicitTransferSyntax);
  if (pcid == 0)
  {
    OFLOG_ERROR(logger,"No presentation context found for sending N-EVENT-REPORT with SOP Class / Transfer Syntax: "
      << dcmFindNameOfUID(UID_StorageCommitmentPushModelSOPClass, "") << "/"
      << DcmXfer(UID_LittleEndianImplicitTransferSyntax).getXferName());
    return DIMSE_NOVALIDPRESENTATIONCONTEXTID;
  }

  return EC_Normal;
}

void DcmStorCmtSCU::closeAssociation(const DcmCloseAssociationType closeType)
{
  OFCondition cond;
  OFString tempStr;

  /* tear down association, i.e. terminate network connection to SCP */
  switch (closeType)
  {
    case DCMSCU_RELEASE_ASSOCIATION:
      /* release association */
      OFLOG_INFO(logger,"Releasing Association");
      cond = ASC_releaseAssociation(m_assoc);
      if (cond.bad())
      {
        OFLOG_ERROR(logger,"Association Release Failed: " << DimseCondition::dump(tempStr, cond));
        return; // TODO: do we really need this?
      }
      break;
    case DCMSCU_ABORT_ASSOCIATION:
      /* abort association */
      OFLOG_INFO(logger,"Aborting Association");
      cond = ASC_abortAssociation(m_assoc);
      if (cond.bad())
      {
        OFLOG_ERROR(logger,"Association Abort Failed: " << DimseCondition::dump(tempStr, cond));
      }
      break;
    case DCMSCU_PEER_REQUESTED_RELEASE:
      /* peer requested release */
      OFLOG_ERROR(logger,"Protocol Error: Peer requested release (Aborting)");
      OFLOG_INFO(logger,"Aborting Association");
      cond = ASC_abortAssociation(m_assoc);
      if (cond.bad())
      {
        OFLOG_ERROR(logger,"Association Abort Failed: " << DimseCondition::dump(tempStr, cond));
      }
      break;
    case DCMSCU_PEER_ABORTED_ASSOCIATION:
      /* peer aborted association */
      OFLOG_INFO(logger,"Peer Aborted Association");
      break;
  }

  /* destroy the association, i.e. free memory of T_ASC_Association* structure. This */
  /* call is the counterpart of ASC_requestAssociation(...) which was called above. */
  cond = ASC_destroyAssociation(&m_assoc);
  if (cond.bad())
  {
    OFLOG_ERROR(logger,"Unable to clean up internal association structures: " << DimseCondition::dump(tempStr, cond));
  }

  /* drop the network, i.e. free memory of T_ASC_Network* structure. This call */
  /* is the counterpart of ASC_initializeNetwork(...) which was called above. */
  cond = ASC_dropNetwork(&m_net);
  if (cond.bad())
  {
    OFLOG_ERROR(logger,"Unable to clean up internal network structures: " << DimseCondition::dump(tempStr, cond));
  }
}

Uint16 DcmStorCmtSCU::nextMessageID()
{
  if (m_assoc == NULL)
    return 0;
  else
    return m_assoc->nextMsgID++;
}

OFBool DcmStorCmtSCU::isConnected() const
{
  return (m_assoc != NULL) && (m_assoc->DULassociation != NULL);
}


