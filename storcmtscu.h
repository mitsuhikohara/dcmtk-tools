#ifndef _STORCMTSCU_H_
#define _STORCMTSCU_H_

#include <dcmtk/dcmnet/scu.h>

class DcmStorCmtSCU: public DcmSCU {

public:
     DcmStorCmtSCU();
     virtual ~DcmStorCmtSCU();

     virtual OFCondition initNetwork();
     virtual OFCondition negotiateAssociation();
     virtual OFCondition sendECHORequest(const T_ASC_PresentationContextID presID);

     virtual OFCondition sendEVENTREPORTRequest(const T_ASC_PresentationContextID presID,
                                        const OFString &sopInstanceUID,
                                        const Uint16 actionTypeID,
                                        DcmDataset *reqDataset,
                                        Uint16 &rspStatusCode);
     virtual void closeAssociation(const DcmCloseAssociationType closeType);

     void setAETitle(const OFString &myAETtitle);

     void setPeerHostName(const OFString &peerHostName);

     void setPeerAETitle(const OFString &peerAETitle);

     void setPeerPort(const Uint16 peerPort);

     OFBool isConnected() const;

private:

  /// Associaton of this SCU. This class only handles 1 association at a time.
  T_ASC_Association *m_assoc;

  /// The DICOM network the association is based on
  T_ASC_Network *m_net;

  /// Association parameters
  T_ASC_Parameters *m_params;

  /// Configuration file for presentation contexts (optional)
  OFString m_assocConfigFilename;

  /// Profile in configuration file that should be used (optional)
  OFString m_assocConfigProfile;

  /// Defines presentation context, consisting of one abstract syntax name
  /// and a list of transfer syntaxes for this abstract syntax
  struct DcmSCUPresContext {
    /// Abstract Syntax Name of Presentation Context
    OFString abstractSyntaxName;
    /// List of Transfer Syntaxes for Presentation Context
    OFList<OFString> transferSyntaxes;
  };

  /// List of presentation contexts that should be negotiated
  OFList<DcmSCUPresContext> m_presContexts;

  /// Configuration file containing association parameters
  OFString m_assocConfigFile;

  /// The last DIMSE successfully sent, unresponded DIMSE request
  T_DIMSE_Message *m_openDIMSERequest;

  /// Maximum PDU size
  Uint32 m_maxReceivePDULength;

  /// DIMSE blocking mode
  T_DIMSE_BlockingMode m_blockMode;

  /// AEtitle of this application
  OFString m_ourAETitle;

  /// Peer hostname
  OFString m_peer;

  /// AEtitle of remote application
  OFString m_peerAETitle;

  /// Port of remote application entity
  Uint16 m_peerPort;

  /// DIMSE timeout
  Uint32 m_dimseTimeout;

  /// ACSE timeout
  Uint32 m_acseTimeout;

  /// Verbose PC mode
  OFBool m_verbosePCMode;

  /** Returns next available message ID free to be used by SCU
   *  @return Next free message ID
   */
  Uint16 nextMessageID();


};



#endif /* _STORCMTSCP_H_ */
