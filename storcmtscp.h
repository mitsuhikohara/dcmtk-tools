#ifndef _STORCMTSCP_H_
#define _STORCMTSCP_H_

#include <dcmtk/dcmnet/scp.h>
#include "storcmtscu.h" 

class DcmStorCmtSCP: public DcmSCP {

public:
     DcmStorCmtSCP();
     virtual ~DcmStorCmtSCP();

     void setPort(const Uint16 port);
     void setAETitle(const OFString &aetitle);
     OFCondition loadAssociationCfgFile(const OFString &assocFile);
     OFCondition listen();

     OFBool isConnected() const;

protected:
     virtual OFCondition waitForAssociation(T_ASC_Network *network);
     virtual void notifyAssociationRequest(const T_ASC_Parameters &params, DcmSCPActionType &desiredAction);

     virtual OFCondition negotiateAssociation();
     virtual void handleAssociation();

     virtual OFCondition handleIncomingCommand(T_DIMSE_Message *msg, const DcmPresentationContextInfo &info);
     virtual OFCondition handleECHORequest(T_DIMSE_C_EchoRQ &reqMessage, T_ASC_PresentationContextID presID);
     virtual OFCondition handleACTIONRequest(T_DIMSE_N_ActionRQ &reqMessage, T_ASC_PresentationContextID presID);

     virtual OFCondition sendEVENTREPORTRequest(const T_ASC_PresentationContextID presID);
     virtual OFCondition handleEVENTREPORTResponse(T_DIMSE_N_EventReportRSP &respMessage, T_ASC_PresentationContextID presID);


private:
//     T_ASC_Network *m_net;

  /// Association parameters
//     T_ASC_Parameters *m_params;

     /// Current association run by this SCP
     T_ASC_Association *m_assoc;

     /// Association configuration. May be filled from association configuration file or by
     /// adding presentation contexts by calling addPresentationContext() (or both)
     DcmAssociationConfiguration *m_assocConfig;

     /// Profile in association configuration that should be used. By default, a profile
     /// called "DEFAULT" is used.
     OFString m_assocCfgProfileName;

     /// Port on which the SCP is listening for association requests. The default port is 104.
     Uint16 m_port;

     /// AETitle to be used for responding to SCU (default: DCMTK_SCP). This value is not
     /// evaluated if the the SCP is configured to respond to any assocation requests with the
     /// name the SCU used as Called AE Title (which is the SCP's default behaviour); see
     /// setRespondWithCalledAETitle().
     OFString m_aetitle;

     /// Maximum PDU size the SCP is able to receive. This value is sent to the SCU during
     /// association negotiation.
     Uint32 m_maxReceivePDULength;

     /// Timeout for DIMSE operations in seconds. Maximum time in DIMSE non-blocking mode to
     /// wait for incoming DIMSE data.
     Uint32 m_dimseTimeout;

     /// Timeout for ACSE operations in seconds. Maximum time during association negotiation
     /// which is given for the SCU to follow the ACSE protocol.
     Uint32 m_acseTimeout;


     DcmStorCmtSCU *m_scu ;
};


#endif /* _STORCMTSCP_H_ */
