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

#ifndef _DSTORCMTSCU_H_
#define _DSTORCMTSCU_H_

#include "dcmtk/config/osconfig.h"  /* make sure OS specific configuration is included first */

#include "dcmtk/dcmdata/dctk.h"     /* covers most common dcmdata classes */
#include "dcmtk/dcmnet/dcompat.h"
#include "dcmtk/dcmnet/dimse.h"     /* DIMSE network layer */
#include "dcmtk/ofstd/oflist.h"

#include <dcmtk/ofstd/ofthread.h>

// include this file in doxygen documentation

/** @file dstorcmtscu.h
 *  @brief Storage Commitment Push Model Service Class User (SCU) class
 */


/** Different types of closing an association
 */
enum DcmCloseAssociationType
{
  /// Release the current association
  DCMSCU_RELEASE_ASSOCIATION,
  /// Abort the current association
  DCMSCU_ABORT_ASSOCIATION,
  /// Peer requested release (Aborting)
  DCMSCU_PEER_REQUESTED_RELEASE,
  /// Peer aborted the association
  DCMSCU_PEER_ABORTED_ASSOCIATION
};

struct DcmStorageCommitmentCommand {

    OFString localAETitle;
    OFString remoteAETitle;
    OFString remoteHostName;
    OFString remoteIP;
    Uint16 remotePort;

    DcmDataset *reqDataset ;

} ;

class DcmStorCmtSCU {

public:
    DcmStorCmtSCU();
    virtual ~DcmStorCmtSCU();

  /** Add presentation context to be used for association negotiation
   *  @param abstractSyntax [in] Abstract syntax name in UID format
   *  @param xferSyntaxes   [in] List of transfer syntaxes to be added for the given abstract
   *                             syntax
   *  @param role           [in] The role to be negotiated
   *  @return EC_Normal if adding was successful, otherwise error code
   */
  OFCondition addPresentationContext(const OFString &abstractSyntax,
                                     const OFList<OFString> &xferSyntaxes,
                                     const T_ASC_SC_ROLE role = ASC_SC_ROLE_DEFAULT);

  /** Initialize network, i.e.\ prepare for association negotiation. If the SCU is already
   *  connected, the call will not be successful and the old connection keeps open.
   *  @return EC_Normal if initialization was successful, otherwise error code.
   *          NET_EC_AlreadyConnected if SCU is already connected.
   */
  virtual OFCondition initNetwork();

 /** Negotiate association by using presentation contexts and parameters as defined by
   *  earlier function calls. If negotiation fails, there is no need to close the association
   *  or to do anything else with this class.
   *  @return EC_Normal if negotiation was successful, otherwise error code.
   *          NET_EC_AlreadyConnected if SCU is already connected.
   */
  virtual OFCondition negotiateAssociation();

  /** After negotiation association, this call returns the first usable presentation context
   *  given the desired abstract syntax and transfer syntax
   *  @param abstractSyntax [in] The abstract syntax (UID) to look for
   *  @param transferSyntax [in] The transfer syntax (UID) to look for. If empty, the transfer
   *                             syntax is not checked.
   *  @return Adequate Presentation context ID that can be used. 0 if none found.
   */
  T_ASC_PresentationContextID findPresentationContextID(const OFString &abstractSyntax,
                                                        const OFString &transferSyntax);

  /** This function sends N-EVENT-REPORT request and receives the corresponding response
   *  @param presID         [in]  The ID of the presentation context to be used for sending
   *                              the request message. Should not be 0.
   *  @param sopInstanceUID [in]  The requested SOP Instance UID
   *  @param eventTypeID    [in]  The event type ID to be used
   *  @param reqDataset     [in]  The request dataset to be sent
   *  @param rspStatusCode  [out] The response status code received. 0 means success,
   *                              others can be found in the DICOM standard.
   *  @return EC_Normal if request could be issued and response was received successfully,
   *          an error code otherwise
   */
  virtual OFCondition sendEVENTREPORTRequest(const T_ASC_PresentationContextID presID,
                                             const OFString &sopInstanceUID,
                                             const Uint16 eventTypeID,
                                             DcmDataset *reqDataset,
                                             Uint16 &rspStatusCode);

  /** Closes the association created by this SCU. Also resets the current association.
   *  @deprecated The use of this method is deprecated. Please use releaseAssociation()
   *    or abortAssociation() instead.
   *  @param closeType [in] Define whether to release or abort the association
   */
  virtual void closeAssociation(const DcmCloseAssociationType closeType);

  /** Releases the current association by sending an A-RELEASE request to the SCP.
   *  Please note that this release only applies to associations that have been
   *  created by calling initNetwork() and negotiateAssociation().
   *  @return status, EC_Normal if successful, an error code otherwise
   */
  virtual OFCondition releaseAssociation();

  /** Aborts the current association by sending an A-ABORT request to the SCP.
   *  Please note that this abort only applies to associations that have been
   *  created by calling initNetwork() and negotiateAssociation().
   *  @return status, EC_Normal if successful, an error code otherwise
   */
  virtual OFCondition abortAssociation();

  /* Set methods */

  /** Set maximum PDU length (to be received by SCU)
   *  @param maxRecPDU [in] The maximum PDU size to use in bytes
   */
  void setMaxReceivePDULength(const Uint32 maxRecPDU);

  /** Set whether to send in DIMSE blocking or non-blocking mode
   *  @param blockingMode [in] Either blocking or non-blocking mode
   */
  void setDIMSEBlockingMode(const T_DIMSE_BlockingMode blockingMode);

  /** Set SCU's AE title to be used in association negotiation
   *  @param myAETtitle [in] The SCU's AE title to be used
   */
  void setAETitle(const OFString &myAETtitle);

  /** Set SCP's host (hostname or IP address) to talk to in association negotiation
   *  @param peerHostName [in] The SCP's hostname or IP address to be used
   */
  void setPeerHostName(const OFString &peerHostName);

  /** Set SCP's AE title to talk to in association negotiation
   *  @param peerAETitle [in] The SCP's AE title to be used
   */
  void setPeerAETitle(const OFString &peerAETitle);

  /** Set SCP's port number to connect to for association negotiation
   *  @param peerPort [in] The SCP's port number
   */
  void setPeerPort(const Uint16 peerPort);

  /** Set whether to show presentation contexts in verbose or debug mode
   *  @param mode [in] Show presentation contexts in verbose mode if OFTrue. By default,
   *                   the presentation contexts are shown in debug mode.
   */
  void setVerbosePCMode(const OFBool mode);

  /* Get methods */

  /** Get current connection status
   *  @return OFTrue if SCU is currently connected, OFFalse otherwise
   */
  OFBool isConnected() const;

  /** Returns the verbose presentation context mode configured specifying whether details
   *  on the presentation contexts (negotiated during association setup) should be shown in
   *  verbose or debug mode. The latter is the default.
   *  @return The current verbose presentation context mode. Show details on the
   *          presentation contexts on INFO log level (verbose) if OFTrue and on DEBUG
   *          level if OFFalse.
   */
  OFBool getVerbosePCMode() const;

  /** Deletes internal networking structures from memory */
  void freeNetwork();

  // set storage commitment command
    void setStorageCommitCommand( DcmStorageCommitmentCommand *command );

protected:

  /** Sends a DIMSE command and possibly also a dataset from a data object via network to
   *  another DICOM application
   *  @param presID     [in]  Presentation context ID to be used for message
   *  @param msg        [in]  Structure that represents a certain DIMSE command which
   *                          shall be sent
   *  @param dataObject [in]  The instance data which shall be sent to the other DICOM
   *                          application; NULL, if there is none
   *  @param commandSet [out] If this parameter is not NULL it will return a copy of the
   *                          DIMSE command which is sent to the other DICOM application
   *  @return EC_Normal if sending request was successful, an error code otherwise
   */
  OFCondition sendDIMSEMessage(const T_ASC_PresentationContextID presID,
                               T_DIMSE_Message *msg,
                               DcmDataset *dataObject,
                               DcmDataset **commandSet = NULL);

  /** Receive DIMSE command (excluding dataset!) over the currently open association
   *  @param presID       [out] Contains in the end the ID of the presentation context
   *                            which was specified in the DIMSE command received
   *  @param msg          [out] The message received
   *  @param statusDetail [out] If a non-NULL value is passed this variable will in the end
   *                            contain detailed information with regard to the status
   *                            information which is captured in the status element
   *                            (0000,0900). Note that the value for element (0000,0900) is
   *                            not contained in this return value but in internal msg. For
   *                            details on the structure of this object, see DICOM standard
   *                            part 7, annex C).
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
                                  T_DIMSE_Message *msg,
                                  DcmDataset **statusDetail,
                                  DcmDataset **commandSet = NULL,
                                  const Uint32 timeout = 0);

  /** Receives one dataset (of instance data) via network from another DICOM application
   *  @param presID     [out] Contains in the end the ID of the presentation context which
   *                          was used in the PDVs that were received on the network. If the
   *                          PDVs show different presentation context IDs, this function
   *                          will return an error.
   *  @param dataObject [out] Contains in the end the information which was received over
   *                          the network
   *  @return EC_Normal if dataset could be received successfully, an error code otherwise
   */
  OFCondition receiveDIMSEDataset(T_ASC_PresentationContextID *presID,
                                  DcmDataset **dataObject);

  /** clear list of presentation contexts. In addition, any currently selected association
   *  configuration file is disabled.
   */
  void clearPresentationContexts();

   /** After negotiation association, this call returns the presentation context belonging
    *  to the given presentation context ID
    *  @param presID         [in]  The presentation context ID to look for
    *  @param abstractSyntax [out] The abstract syntax (UID) for that ID.
    *                              Empty, if such a presentation context does not exist.
    *  @param transferSyntax [out] The transfer syntax (UID) for that ID.
    *                              Empty, if such a presentation context does not exist.
    */
  void findPresentationContext(const T_ASC_PresentationContextID presID,
                               OFString &abstractSyntax,
                               OFString &transferSyntax);

private:

    // private undefined copy constructor
    DcmStorCmtSCU(const DcmStorCmtSCU &);

    // private undefined assignment operator
    DcmStorCmtSCU &operator=(const DcmStorCmtSCU &);

    // Storage commit command to send in EVENT REPORT
    DcmStorageCommitmentCommand *storageCommitCommand;

  /// Association of this SCU. This class only handles 1 association at a time.
  T_ASC_Association *m_assoc;

  /// The DICOM network the association is based on
  T_ASC_Network *m_net;

  /// Association parameters
  T_ASC_Parameters *m_params;

  /// Defines presentation context, consisting of one abstract syntax name
  /// and a list of transfer syntaxes for this abstract syntax
  struct DCMTK_DCMNET_EXPORT DcmSCUPresContext {
    /** Default constructor
     */
    DcmSCUPresContext()
    : abstractSyntaxName()
    , transferSyntaxes()
    , roleSelect(ASC_SC_ROLE_DEFAULT)
    {
    }
    /// Abstract Syntax Name of Presentation Context
    OFString abstractSyntaxName;
    /// List of Transfer Syntaxes for Presentation Context
    OFList<OFString> transferSyntaxes;
    /// Role Selection
    T_ASC_SC_ROLE roleSelect;
  };

  /// List of presentation contexts that should be negotiated
  OFList<DcmSCUPresContext> m_presContexts;

  /// Maximum PDU size (default: 16384 bytes)
  Uint32 m_maxReceivePDULength;

  /// DIMSE blocking mode (default: blocking)
  T_DIMSE_BlockingMode m_blockMode;

  /// AE title of this application (default: ANY-SCU)
  OFString m_ourAETitle;

  /// Peer hostname
  OFString m_peer;

  /// AE title of remote application (default: ANY-SCP)
  OFString m_peerAETitle;

  /// Port of remote application entity (default: 104)
  Uint16 m_peerPort;

  /// DIMSE timeout (default: unlimited)
  Uint32 m_dimseTimeout;

  /// ACSE timeout (default: 30 seconds)
  Uint32 m_acseTimeout;

  /// Verbose PC mode (default: disabled)
  OFBool m_verbosePCMode;

  /** Returns next available message ID free to be used by SCU
   *  @return Next free message ID
   */
  Uint16 nextMessageID();

};



#endif /* _DSTORCMTSCP_H_ */
