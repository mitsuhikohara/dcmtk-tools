# dcmtk

This is experimental develop the dicom scp using dcmtk
It is still incomplete.

1. Modality Performed Procedure Step SCP

   - receive N-CREATE Request and send back N-CREATE Response
   - receive N-SET Request and send back N-SET Response

2. Storage Commitment SCP

   - receive N-ACTION Request and send back N-ACTION Response
   - send N-EVENT-REPORT Request in the same association with N-ACTION Response
     if association is closed within 5 sec, otherwise send N-EVENT-REPORT Request 
     in new association.

Requirement: 
    dcmtk-3.6.1 is required to built
    It is only tested on OpenSUSE Leap 42

Usage:

    % mppsrecv -aet <AETitle> <port number>
    
    % storcmtrecv  -aet <AETitle> < port number> 




