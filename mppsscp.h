#ifndef _MPPSSCP_H_
#define _MPPSSCP_H_

#include <dcmtk/dcmnet/scp.h>

class DcmMppsSCP: public DcmSCP {

public:
     DcmMppsSCP();
     virtual ~DcmMppsSCP();

     void setPort(const Uint16 port);
     void setAETitle(const OFString &aetitle);
     OFCondition loadAssociationCfgFile(const OFString &assocFile);
     OFCondition listen();

protected:
     virtual OFCondition waitForAssociation(T_ASC_Network *network);
     virtual void notifyAssociationRequest(const T_ASC_Parameters &params, DcmSCPActionType &desiredAction);

     virtual OFCondition negotiateAssociation();
     virtual void handleAssociation();
     virtual OFCondition handleIncomingCommand(T_DIMSE_Message *msg, const DcmPresentationContextInfo &info);
     virtual OFCondition handleNCREATERequest(T_DIMSE_N_CreateRQ &reqMessage, T_ASC_PresentationContextID presID);
     virtual OFCondition handleNSETRequest(T_DIMSE_N_SetRQ &reqMessage, T_ASC_PresentationContextID presID);

private:
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

};


#endif /* _MPPSSCP_H_ */
