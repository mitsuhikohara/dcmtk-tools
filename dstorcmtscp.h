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

#include "dcmtk/oflog/oflog.h"
#include "dcmtk/dcmdata/dctk.h"     /* Covers most common dcmdata classes */
#include "dcmtk/dcmnet/assoc.h"
#include "dcmtk/dcmnet/dimse.h"     /* DIMSE network layer */
#include "dcmtk/dcmnet/scpcfg.h"
#include "dcmtk/dcmnet/diutil.h"    /* for DCMNET_WARN() */

//#include "dcmtk/dcmnet/scp.h"       /* for base class DcmSCP */
#include "dstorcmtscu.h"



/** Action codes that can be given to DcmSCP to control behavior during SCP's operation.
 *  Different hooks permit jumping into different phases of SCP operation.
 */
enum DcmSCPActionType
{
  /// No action defined
  DCMSCP_ACTION_UNDEFINED,
  /// Tell SCP to refuse association
  DCMSCP_ACTION_REFUSE_ASSOCIATION
};

/** Codes denoting a reason for refusing an association
 */
enum DcmRefuseReasonType
{
  /// Too many associations (SCP cannot handle a further association)
  DCMSCP_TOO_MANY_ASSOCIATIONS,
  /// Forking a new SCP process failed
  DCMSCP_CANNOT_FORK,
  /// Refusing association because of bad application context name
  DCMSCP_BAD_APPLICATION_CONTEXT_NAME,
  /// Refusing association because of unaccepted called AE title
  DCMSCP_CALLED_AE_TITLE_NOT_RECOGNIZED,
  /// Refusing association because of unaccepted calling AE title
  DCMSCP_CALLING_AE_TITLE_NOT_RECOGNIZED,
  /// Refusing association because SCP was forced to do so
  DCMSCP_FORCED,
  /// Refusing association because of missing Implementation Class UID
  DCMSCP_NO_IMPLEMENTATION_CLASS_UID,
  /// Refusing association because of no acceptable Presentation Contexts
  DCMSCP_NO_PRESENTATION_CONTEXTS,
  /// Refusing association because of internal error
  DCMSCP_INTERNAL_ERROR
};

/** Structure representing a single Presentation Context. Fields "reserved" and "result"
 *  not included from DUL_PRESENTATIONCONTEXT, which served as the blueprint for this
 *  structure.
 */
struct DCMTK_DCMNET_EXPORT DcmPresentationContextInfo
{
  DcmPresentationContextInfo()
    : presentationContextID(0)
    , abstractSyntax()
    , proposedSCRole(0)
    , acceptedSCRole(0)
    , acceptedTransferSyntax()
  {
  }

  /// Presentation Context ID as proposed by SCU
  Uint8 presentationContextID;
  /// Abstract Syntax name (UID) as proposed by SCU
  OFString abstractSyntax;
  /// SCP role as proposed from SCU
  Uint8 proposedSCRole;
  /// Role accepted by SCP for this Presentation Context
  Uint8 acceptedSCRole;
  /// Transfer Syntax accepted for this Presentation Context (UID)
  OFString acceptedTransferSyntax;
  // Fields "reserved" and "result" not included from DUL_PRESENTATIONCONTEXT
};

/*---------------------*
 *  class declaration  *
 *---------------------*/

/** Interface class for a Storage Commiment Push Model Service Class Provider (SCP).
 *  This class supports N-ACTION, N-EVENT-REPORT  and C-ECHO messages as an SCP.
 *  @note The current implementation send N-EVENT-REPORT in the same association as
 *  N-ACTION
 */
//class DCMTK_DCMNET_EXPORT DcmStorCmtSCP : public DcmSCP
class DCMTK_DCMNET_EXPORT DcmStorCmtSCP 
{

  public:

    /** default constructor
     */
    DcmStorCmtSCP();

    /** destructor
     */
    virtual ~DcmStorCmtSCP();

  /** Starts providing the implemented services to SCUs.
   *  After calling this method the SCP is listening for connection requests.
   *  @return The result. Function usually only returns in case of errors.
   */
  virtual OFCondition listen();

  /* ************************************************************* */
  /*             Set methods for configuring SCP behavior          */
  /* ************************************************************* */

  /** Add abstract syntax to presentation contexts the SCP is able to negotiate with SCUs.
   *  @param abstractSyntax [in] The UID of the abstract syntax (e.g.\ SOP class) to add
   *  @param xferSyntaxes   [in] List of transfer syntaxes (UIDs) that should be supported
   *                             for the given abstract syntax name
   *  @param role           [in] The role to be negotiated
   *  @param profile        [in] The profile the abstract syntax should be added to. The
   *                             default is to add it to the DcmSCP's internal standard
   *                             profile called "DEFAULT".
   *  @return EC_Normal if adding was successful, an error code otherwise
   */
  virtual OFCondition addPresentationContext(const OFString &abstractSyntax,
                                             const OFList<OFString> &xferSyntaxes,
                                             const T_ASC_SC_ROLE role = ASC_SC_ROLE_DEFAULT,
                                             const OFString &profile = "DEFAULT");

  /** Set SCP's TCP/IP listening port
   *  @param port [in] The port number to listen on. Note that usually on Unix-like systems
   *                   only root user is permitted to open ports below 1024.
   */
  void setPort(const Uint16 port);

  /** Set SCU's TCP/IP listening port
   *  @param port [in] The port number to listen on. Note that usually on Unix-like systems
   *                   only root user is permitted to open ports below 1024.
   */
  void setPeerPort(const Uint16 port);

  /** Set AE title of the server
   *  @param aetitle [in] The AE title of the server. By default, all SCU association requests
   *                      calling another AE title will be rejected. This behavior can be
   *                      changed by using the setRespondWithCalledAETitle() method.
   */
  void setAETitle(const OFString &aetitle);

  /** Set SCP to use the called AE title from the SCU request for the response, i.e.\ the SCP
   *  will always respond with setting it's own name to the one the SCU used for calling.
   *  Overrides any AE title eventually set with setAETitle().
   *  @param useCalled [in] If OFTrue, the SCP will use the called AE title from the request
   *                        for responding. DcmSCP's default is OFFalse.
   */
  void setRespondWithCalledAETitle(const OFBool useCalled);

  /** Set maximum PDU size the SCP is able to receive. This size is sent in association
   *  response message to SCU.
   *  @param maxRecPDU [in] The maximum PDU size to use in bytes
   */
  void setMaxReceivePDULength(const Uint32 maxRecPDU);

  /** Set whether waiting for a TCP/IP connection should be blocking or non-blocking.
   *  In non-blocking mode, the networking routines will wait for specified connection
   *  timeout, see setConnectionTimeout() function. In blocking mode, no timeout is set
   *  but the operating system's network routines will be used to read from the socket
   *  for incoming data. In the worst case, this may be a long time until that call
   *  returns. The default of DcmSCP is blocking mode.
   *  @param blockingMode [in] Either DUL_BLOCK for blocking mode or DUL_NOBLOCK
   *                           for non-blocking mode
   */
  void setConnectionBlockingMode(const DUL_BLOCKOPTIONS blockingMode);

  /** Set whether DIMSE messaging should be blocking or non-blocking. In non-blocking mode,
   *  the networking routines will wait for DIMSE messages for the specified DIMSE timeout
   *  time, see setDIMSETimeout() function. In blocking mode, no timeout is set but the
   *  operating system's network routines will be used to read from the socket for new data.
   *  In the worst case, this may be a long time until that call returns. The default of
   *  DcmSCP is blocking mode.
   *  @param blockingMode [in] Either DIMSE_BLOCKING for blocking mode or DIMSE_NONBLOCKING
   *                           for non-blocking mode
   */
  void setDIMSEBlockingMode(const T_DIMSE_BlockingMode blockingMode);

  /** Set the timeout to be waited for incoming DIMSE message packets. This is only relevant
   *  for DIMSE blocking mode messaging (see also setDIMSEBlockingMode()).
   *  @param dimseTimeout [in] DIMSE receive timeout in seconds
   */
  void setDIMSETimeout(const Uint32 dimseTimeout);

  /** Set the timeout used during ACSE messaging protocol.
   *  @param acseTimeout [in] ACSE timeout in seconds.
   */
  void setACSETimeout(const Uint32 acseTimeout);

  /** Set the timeout that should be waited for connection requests.
   *  Only relevant in non-blocking mode (default).
   *  @param timeout [in] TCP/IP connection timeout in seconds.
   */
  void setConnectionTimeout(const Uint32 timeout);

  /** Set whether to show presentation contexts in verbose or debug mode
   *  @param mode [in] Show presentation contexts in verbose mode if OFTrue. By default, the
   *                   presentation contexts are shown in debug mode.
   */
  void setVerbosePCMode(const OFBool mode);

  /** Enables or disables looking up the host name from a connecting system.
   *  Note that this sets a GLOBAL flag in DCMTK, i.e. the behavior changes
   *  for all servers. This should be changed in the future.
   *  @param mode [in] OFTrue, if hostname lookup should be enabled, OFFalse for disabling it.
   */
  void setHostLookupEnabled(const OFBool mode);

  /** Set maximum commitment event wait delay time in second 
   *  Note: SCP wait for association release request from SCU after ACTION response is sent
   *  @param delay [in]  maximum event delay time in sec
  */
  void setCommitWaitTimeout(const Uint32 timeout);

  /* Get methods for SCP settings */

  /** Returns TCP/IP port number SCP listens for new connection requests
   *  @return The port number
   */
  Uint16 getPort() const;

  /** Returns SCP's own AE title. Only used if the SCP is not configured to respond with the
   *  called AE title the SCU uses for association negotiation, see setRespondWithCalledAETitle().
   *  @return The configured AE title
   */
  const OFString &getAETitle() const;

  /** Returns whether SCP uses the called AE title from SCU requests to respond to connection
   *  requests instead of a configured AE title
   *  @return OFTrue, if the SCU's calling AE title is utilized, OFFalse otherwise
   */
  OFBool getRespondWithCalledAETitle() const;

  /** Returns maximum PDU length configured to be received by SCP
   *  @return Maximum PDU length in bytes
   */
  Uint32 getMaxReceivePDULength() const;

  /** Returns whether receiving of TCP/IP connection requests is done in blocking or
   *  unblocking mode
   *  @return DUL_BLOCK if in blocking mode, otherwise DUL_NOBLOCK
   */
  DUL_BLOCKOPTIONS getConnectionBlockingMode() const;

  /** Returns whether receiving of DIMSE messages is done in blocking or unblocking mode
   *  @return DIMSE_BLOCKING if in blocking mode, otherwise DIMSE_NONBLOCKING
   */
  T_DIMSE_BlockingMode getDIMSEBlockingMode() const;

  /** Returns DIMSE timeout (only applicable in blocking mode)
   *  @return DIMSE timeout in seconds
   */
  Uint32 getDIMSETimeout() const;

  /** Returns ACSE timeout
   *  @return ACSE timeout in seconds
   */
  Uint32 getACSETimeout() const;

  /** Returns connection timeout
   *  @return TCP/IP connection timeout in seconds
   */
  Uint32 getConnectionTimeout() const;

  /** Returns the verbose presentation context mode configured specifying whether details on
   *  the presentation contexts (negotiated during association setup) should be shown in
   *  verbose or debug mode. The latter is the default.
   *  @return The verbose presentation context mode configured
   */
  OFBool getVerbosePCMode() const;

  /** Returns whether a connecting system's host name is looked up.
   *  @return OFTrue, if hostname lookup is enabled, OFFalse otherwise
   */
  OFBool getHostLookupEnabled() const;

  /* ************************************************************* */
  /*  Methods for receiving runtime (i.e. connection time) infos   */
  /* ************************************************************* */

  /** Returns whether SCP is currently connected. If in multi-process mode, the "father"
   *  process should always return false here, because connection is always handled by child
   *  process.
   *  @return OFTrue, if SCP is currently connected to calling SCU
   */
  OFBool isConnected() const;

  /** Returns AE title the SCU used as called AE title in association request
   *  @return AE title the SCP was called with. Empty string if SCP is currently not
   *          connected.
   */
  OFString getCalledAETitle() const;

  /** Returns AE title (calling AE title) the SCU used for association request
   *  @return Calling AE title of SCU. Empty string if SCP is currently not connected.
   */
  OFString getPeerAETitle() const;

  /** Returns IP address of connected SCU
   *  @return IP address of connected SCU. Empty string if SCP is currently not connected.
   */
  OFString getPeerIP() const;

  /** Returns Port number of connected SCU
   *  @return Port number of connected SCU.
   *  Note : it is fixed value given
   */
  Uint16 getPeerPort() const;

  /** Returns maximum PDU size the communication peer (i.e.\ the SCU) is able to receive
   *  @return Maximum PDU size the SCU is able to receive. Returns zero if SCP is currently
   *          not connected.
   */
  Uint32 getPeerMaxPDULength() const;

  /** Returns maximum commitment event wait delay time in second 
   *  @return  maximum event delay time in sec
  */
  Uint32  getCommitWaitTimeout() const;

  protected:

  /* ********************************************* */
  /*  Functions available to derived classes only  */
  /* ********************************************* */

  /** This call returns the presentation context belonging to the given
   *  presentation context ID.
   *  @param presID         [in]  The presentation context ID to look for
   *  @param abstractSyntax [out] The abstract syntax (UID) for that ID.
   *                              Empty, if such a presentation context does not exist.
   *  @param transferSyntax [out] The transfer syntax (UID) for that ID.
   *                              Empty, if such a presentation context does not exist.
   */
  void findPresentationContext(const T_ASC_PresentationContextID presID,
                               OFString &abstractSyntax,
                               OFString &transferSyntax);

  /** Aborts the current association by sending an A-ABORT request to the SCU.
   *  This method allows derived classes to abort an association in case of severe errors.
   *  @return status, EC_Normal if successful, an error code otherwise
   */
  virtual OFCondition abortAssociation();

    /** handler that is called for each incoming command message.  This handler supports
     *  C-ECHO, N-ACTION requests.  All other messages will be reported as an error.
     *  @param  incomingMsg  pointer to data structure containing the DIMSE message
     *  @param  presInfo     additional information on the Presentation Context used
     *  @return status, EC_Normal if successful, an error code otherwise
     */
    virtual OFCondition handleIncomingCommand(T_DIMSE_Message *incomingMsg,
                                              const DcmPresentationContextInfo &presInfo);

  /** Overwrite this function to be notified about an incoming association request.
   *  The standard handler only outputs some information to the logger.
   *  @param params The association parameters that were received.
   *  @param desiredAction The desired action how to handle this association request.
   */
  virtual void notifyAssociationRequest(const T_ASC_Parameters &params,
                                        DcmSCPActionType &desiredAction);

  /** Overwrite this function if called AE title should undergo checking. If
   *  OFTrue is returned, the AE title is accepted and processing is continued.
   *  In case of OFFalse, the SCP will refuse the incoming association with
   *  error "Called Application Entity Title Not Recognized".
   *  The standard handler always returns OFTrue.
   *  @param calledAE The called AE title the SCU used that should be checked
   *  @return OFTrue, if AE title is accepted, OFFalse otherwise
   */
  virtual OFBool checkCalledAETitleAccepted(const OFString& calledAE);

  /** Overwrite this function if calling AE title should undergo checking. If
   *  OFTrue is returned, the AE title is accepted and processing is continued.
   *  In case of OFFalse, the SCP will refuse the incoming association with
   *  error "Calling Application Entity Title Not Recognized".
   *  The standard handler always returns OFTrue.
   *  @param callingAE The calling AE title the SCU used that should be checked
   *  @return OFTrue, if AE title is accepted, OFFalse otherwise
   */
  virtual OFBool checkCallingAETitleAccepted(const OFString& callingAE);

  /** Overwrite this function if calling IP should undergo checking. Note
   *  that this function may also return a hostname instead. If
   *  OFTrue is returned, the IP is accepted and processing is continued.
   *  In case of OFFalse, the SCP will refuse the incoming association with
   *  an error. The standard handler always returns OFTrue.
   *  @param hostOrIP The IP of the client to check.
   *  @return OFTrue, if IP/host is accepted, OFFalse otherwise
   */
  virtual OFBool checkCallingHostAccepted(const OFString& hostOrIP);


  /** Overwrite this function to be notified about an incoming association request.
   *  The standard handler only outputs some information to the logger.
   */
  virtual void notifyAssociationAcknowledge();

  /** Overwrite this function to be notified about an incoming association release request.
   *  The standard handler only outputs some information to the logger.
   */
  virtual void notifyReleaseRequest();

  /** Overwrite this function to be notified about an incoming association abort request.
   *  The standard handler only outputs some information to the logger.
   */
  virtual void notifyAbortRequest();

  /** Overwrite this function to be notified when an association is terminated.
   *  The standard handler only outputs some information to the logger.
   */
  virtual void notifyAssociationTermination();

  /** Overwrite this function to be notified when a DIMSE error occurs.
   *  The standard handler only outputs error information to the logger.
   *  @param cond [in] The DIMSE error occurred.
   */
  virtual void notifyDIMSEError(const OFCondition &cond);

  /** Overwrite this function to change the behavior of the listen() method. As long as no
   *  severe error occurs and this method returns OFFalse, the listen() method will wait
   *  for incoming associations in an infinite loop.
   *  @return The standard handler always returns OFFalse
   */
  virtual OFBool stopAfterCurrentAssociation();

  // -- C-ECHO --

  /** Standard handler for Verification Service Class (DICOM Echo). Returns echo response
   *  (i.e. whether C-ECHO could be responded to with status success).
   *  @param reqMessage [in] The C-ECHO request message that was received
   *  @param presID     [in] The presentation context to be used. By default, the
   *                         presentation context of the request is used.
   *  @return status, EC_Normal if successful, an error code otherwise
   */
  virtual OFCondition handleECHORequest(T_DIMSE_C_EchoRQ &reqMessage,
                                        const T_ASC_PresentationContextID presID);

  /** Receive N-ACTION request on the currently opened association.
   *  @param reqMessage   [in]  The N-ACTION request message that was received
   *  @param presID       [in]  The presentation context to be used. By default, the
   *                            presentation context of the request is used.
   *  @param reqDataset   [out] Pointer to the dataset received
   *  @param actionTypeID [out] Action Type ID from the command set received
   *  @return status, EC_Normal if successful, an error code otherwise
   */
  virtual OFCondition receiveACTIONRequest(T_DIMSE_N_ActionRQ &reqMessage,
                                           const T_ASC_PresentationContextID presID,
                                           DcmDataset *&reqDataset,
                                           Uint16 &actionTypeID);

  /** Receive N-ACTION request on the currently opened association. This
   *  function is deprecated and will be removed in the future. For now it calls
   *  receiveACTIONRequest() which should be used instead.
   *  @param reqMessage   [in]  The N-ACTION request message that was received
   *  @param presID       [in]  The presentation context to be used. By default, the
   *                            presentation context of the request is used.
   *  @param reqDataset   [out] Pointer to the dataset received
   *  @param actionTypeID [out] Action Type ID from the command set received
   *  @return status, EC_Normal if successful, an error code otherwise
   */
  virtual OFCondition handleACTIONRequest(T_DIMSE_N_ActionRQ &reqMessage,
                                          const T_ASC_PresentationContextID presID,
                                          DcmDataset *&reqDataset,
                                          Uint16 &actionTypeID)
  {
    DCMNET_WARN("handleACTIONRequest() is deprecated, use receiveACTIONRequest() instead");
    return receiveACTIONRequest(reqMessage, presID, reqDataset, actionTypeID);
  }

  /** Respond to the N-ACTION request
   *  @param presID         [in] The presentation context ID to respond to
   *  @param messageID      [in] The message ID being responded to
   *  @param sopClassUID    [in] The affected SOP class UID
   *  @param sopInstanceUID [in] The affected SOP instance UID
   *  @param rspStatusCode  [in] The response status code. 0 means success,
   *                             others can found in the DICOM standard.
   *  @return EC_Normal, if responding was successful, an error code otherwise
   */
  virtual OFCondition sendACTIONResponse(const T_ASC_PresentationContextID presID,
                                         const Uint16 messageID,
                                         const OFString &sopClassUID,
                                         const OFString &sopInstanceUID,
                                         const Uint16 rspStatusCode);

  // --- N-EVENT-REPORT  --

  /** Send N-EVENT-REPORT request on the current association and receive a corresponding
   *  response.
   *  @param presID         [in]  The ID of the presentation context to be used for sending
   *                              the request message. Should not be 0.
   *  @param sopInstanceUID [in]  The requested SOP Instance UID
   *  @param messageID      [in]  The request message ID
   *  @param eventTypeID    [in]  The event type ID to be used
   *  @param reqDataset     [in]  The request dataset to be sent
   *  @param rspStatusCode  [out] The response status code received. 0 means success,
   *                              others can be found in the DICOM standard.
   *  @return EC_Normal if request could be issued and response was received successfully,
   *          an error code otherwise
   */
  virtual OFCondition sendEVENTREPORTRequest(const T_ASC_PresentationContextID presID,
                                             const OFString &sopInstanceUID,
                                             const Uint16 messageID,
                                             const Uint16 eventTypeID,
                                             DcmDataset *reqDataset,
                                             Uint16 &rspStatusCode);
  /* ********************************************************************* */
  /*  Further functions and member variables                               */
  /* ********************************************************************* */

  /** Helper function to return presentation context information by given
   *  presentation context ID.
   *  @param head The presentation context list
   *  @param presentationContextID The presentation context ID
   *  @return The presentation context information
   */
  static DUL_PRESENTATIONCONTEXT* findPresentationContextID(LST_HEAD *head,
                                                            T_ASC_PresentationContextID presentationContextID);

  /** Helper function to return presentation context information by given
   *  presentation context ID.
   *  @param assoc The association to search
   *  @param presID The presentation context ID
   *  @param presInfo The result presentation context information, if found
   *  @return OFTrue if presentation context with ID could be found, OFFalse
   *          otherwise
   */
  static OFBool getPresentationContextInfo(const T_ASC_Association *assoc,
                                           const Uint8 presID,
                                           DcmPresentationContextInfo &presInfo);

  /** This function takes care of receiving, negotiating and accepting/refusing an
   *  association request. Additionally, if negotiation was successful, it handles any
   *  incoming DIMSE commands by calling handleAssociation(). An error is only returned, if
   *  something goes wrong. Therefore, refusing an association because of wrong application
   *  context name or no common presentation contexts with the SCU does NOT lead to an error.
   *  @param network [in] Contains network parameters
   *  @return EC_Normal, if everything went fine, an error code otherwise
   */
  virtual OFCondition waitForAssociationRQ(T_ASC_Network *network);

  /** Actually process association request.
   *  @return EC_Normal if association could be processed, error otherwise.
   */
  virtual OFCondition processAssociationRQ();

 /** This function checks all presentation contexts proposed by the SCU whether they are
  *  supported or not. It is not an error if no common presentation context could be
  *  identified with the SCU; only issues like problems in memory management etc. are
  *  reported as an error. This function does not send a response message to the SCU. This
  *  is done in other functions.
  *  @return EC_Normal if negotiation was successfully done, an error code otherwise
  */
  virtual OFCondition negotiateAssociation();

  /** This function takes care of refusing an association request
   *  @param reason [in] The reason why the association request will be refused and that
   *                     will be reported to the SCU.
   */
  virtual void refuseAssociation(const DcmRefuseReasonType reason);

  /** This function takes care of handling the other DICOM application's request. After
   *  having accomplished all necessary steps, the association will be dropped and destroyed.
   */
  virtual void handleAssociation();

  /** Send a DIMSE command and possibly also a dataset from a data object via network to
   *  another DICOM application
   *  @param presID          [in]  Presentation context ID to be used for message
   *  @param message         [in]  Structure that represents a certain DIMSE command which
   *                               shall be sent
   *  @param dataObject      [in]  The instance data which shall be sent to the other DICOM
   *                               application; NULL, if there is none
   *  @param statusDetail    [in]  The status detail of the response (if desired).
   *  @param commandSet      [out] If this parameter is not NULL it will return a copy of the
   *                               DIMSE command which is sent to the other DICOM application
   *  @return Returns EC_Normal if sending request was successful, an error code otherwise
   */
  OFCondition sendDIMSEMessage(const T_ASC_PresentationContextID presID,
                               T_DIMSE_Message *message,
                               DcmDataset *dataObject,
                               DcmDataset *statusDetail = NULL,
                               DcmDataset **commandSet = NULL);

  /** Receive DIMSE command (excluding dataset!) over the currently open association
   *  @param presID       [out] Contains in the end the ID of the presentation context
   *                            which was specified in the DIMSE command received
   *  @param message      [out] The message received
   *  @param statusDetail [out] If a non-NULL value is passed this variable will in the end
   *                            contain detailed information with regard to the status
   *                            information which is captured in the status element
   *                            (0000,0900). Note that the value for element (0000,0900) is
   *                            not contained in this return value but in internal message.
   *                            For details on the structure of this object, see DICOM
   *                            standard part 7, annex C).
   *  @param commandSet   [out] If this parameter is not NULL, it will return a copy of the
   *                            DIMSE command which was received from the other DICOM
   *                            application. The caller is responsible to de-allocate the
   *                            returned object!
   *  @param timeout      [in]  If this parameter is not 0, it specifies the timeout (in
   *                            seconds) to be used for receiving the DIMSE command.
   *                            Otherwise, the default timeout value is used (see
   *                            setDIMSETimeout()).
   *  @return EC_Normal if command could be received successfully, an error code otherwise
   */
  OFCondition receiveDIMSECommand(T_ASC_PresentationContextID *presID,
                                  T_DIMSE_Message *message,
                                  DcmDataset **statusDetail,
                                  DcmDataset **commandSet = NULL,
                                  const Uint32 timeout = 0);

  /** Receive one dataset (of instance data) via network from another DICOM application
   *  @param presID     [out]   Contains in the end the ID of the presentation context
   *                            which was used in the PDVs that were received on the
   *                            network. If the PDVs show different presentation context
   *                            IDs, this function will return an error.
   *  @param dataObject [inout] Contains in the end the information that was received
   *                            over the network. If this parameter points to NULL, a new
   *                            dataset will be created by the underlying routines, which
   *                            has to be deleted by the caller.
   *  @return EC_Normal if dataset could be received successfully, an error code otherwise
   */
  OFCondition receiveDIMSEDataset(T_ASC_PresentationContextID *presID,
                                  DcmDataset **dataObject);

private:

  /// Current association run by this SCP
  T_ASC_Association *m_assoc;

  /// SCP configuration. The configuration is a shared object since in some scenarios one
  /// might like to share a single configuration instance with multiple SCPs without copying
  /// it, e.g. in the context of the DcmSCPPool class.
  DcmSharedSCPConfig m_cfg;

  /** Drops association and clears internal structures to free memory
   */
  void dropAndDestroyAssociation();

    // private undefined copy constructor
    DcmStorCmtSCP(const DcmStorCmtSCP &);

    // private undefined assignment operator
    DcmStorCmtSCP &operator=(const DcmStorCmtSCP &);

    // Storage commit command to send in EVENT REPORT
    DcmStorageCommitmentCommand *storageCommitCommand;

    // commitment wait delay in SCU
    Uint32  m_commit_wait_timeout;

    // peer port of SCU
    Uint16 m_peerPort;
};

#endif // DSTORCMTSCP_H
