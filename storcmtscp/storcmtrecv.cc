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
 *  Purpose: Simple Storage Service Class Provider
 *
 */


#include "dcmtk/config/osconfig.h"   /* make sure OS specific configuration is included first */

#include "dcmtk/ofstd/ofstd.h"       /* for OFStandard functions */
#include "dcmtk/ofstd/ofconapp.h"    /* for OFConsoleApplication */
#include "dcmtk/ofstd/ofstream.h"    /* for OFStringStream et al. */
#include "dcmtk/dcmdata/dcdict.h"    /* for global data dictionary */
#include "dcmtk/dcmdata/dcuid.h"     /* for dcmtk version name */
#include "dcmtk/dcmdata/cmdlnarg.h"  /* for prepareCmdLineArgs */
#include "dstorcmtscp.h"   /* for DcmStorCmtSCP */


/* general definitions */

#define OFFIS_CONSOLE_APPLICATION "storcmtrecv"

static OFLogger dcmrecvLogger = OFLog::getLogger("dcmtk.apps." OFFIS_CONSOLE_APPLICATION);

static char rcsid[] = "$dcmtk: " OFFIS_CONSOLE_APPLICATION " v"
  OFFIS_DCMTK_VERSION " " OFFIS_DCMTK_RELEASEDATE " $";

/* default application entity title */
#define APPLICATIONTITLE "STORCMTSCP"


/* exit codes for this command line tool */
/* (EXIT_SUCCESS and EXIT_FAILURE are standard codes) */

// general
#define EXITCODE_NO_ERROR                         0

// network errors
#define EXITCODE_CANNOT_START_SCP_AND_LISTEN     64


/* helper macro for converting stream output to a string */
#define CONVERT_TO_STRING(output, string) \
    optStream.str(""); \
    optStream.clear(); \
    optStream << output << OFStringStream_ends; \
    OFSTRINGSTREAM_GETOFSTRING(optStream, string)


/* main program */

#define SHORTCOL 4
#define LONGCOL 21

int main(int argc, char *argv[])
{
    OFOStringStream optStream;

    const char *opt_aeTitle = APPLICATIONTITLE;

    OFCmdUnsignedInt opt_port = 0;
    OFCmdUnsignedInt opt_peerPort = 115; // this is default for DVTK
    OFCmdUnsignedInt opt_dimseTimeout = 0;
    OFCmdUnsignedInt opt_acseTimeout = 30;
    OFCmdUnsignedInt opt_maxPDULength = ASC_DEFAULTMAXPDU;
    T_DIMSE_BlockingMode opt_blockingMode = DIMSE_BLOCKING;
    OFCmdUnsignedInt opt_commitWaitTimeout = 5;

    OFBool opt_showPresentationContexts = OFFalse;  // default: do not show presentation contexts in verbose mode
    OFBool opt_useCalledAETitle = OFFalse;          // default: respond with specified application entity title
    OFBool opt_HostnameLookup = OFTrue;             // default: perform hostname lookup (for log output)

    OFConsoleApplication app(OFFIS_CONSOLE_APPLICATION , "Simple DICOM MPPS SCP (receiver)", rcsid);
    OFCommandLine cmd;

    cmd.setParamColumn(LONGCOL + SHORTCOL + 4);
    cmd.addParam("port", "tcp/ip port number to listen on");

    cmd.setOptionColumns(LONGCOL, SHORTCOL);
    cmd.addGroup("general options:", LONGCOL, SHORTCOL + 2);
      cmd.addOption("--help",                  "-h",      "print this help text and exit", OFCommandLine::AF_Exclusive);
      cmd.addOption("--version",                          "print version information and exit", OFCommandLine::AF_Exclusive);
      OFLog::addOptions(cmd);
      cmd.addOption("--verbose-pc",            "+v",      "show presentation contexts in verbose mode");

    cmd.addGroup("network options:");
      cmd.addSubGroup("application entity title:");
        CONVERT_TO_STRING("set my AE title (default: " << opt_aeTitle << ")", optString1);
        cmd.addOption("--aetitle",             "-aet", 1, "[a]etitle: string", optString1.c_str());
        cmd.addOption("--use-called-aetitle",  "-uca",    "always respond with called AE title");
      cmd.addSubGroup("storage commitment options:");
        CONVERT_TO_STRING("[s]econds: integer (default: " << opt_commitWaitTimeout << ")", optString2);
        cmd.addOption("--commit-wait-timeout", "-cwt", 1, optString2.c_str(), "timeout for storage commitment event");
        CONVERT_TO_STRING("port number: integer (default: " << opt_peerPort << ")", optString3);
        cmd.addOption("--peer-port", "-p", 1,  optString3.c_str(), "peer port number");
      cmd.addSubGroup("other network options:");
        CONVERT_TO_STRING("[s]econds: integer (default: " << opt_acseTimeout << ")", optString4);
        cmd.addOption("--acse-timeout",        "-ta",  1, optString4.c_str(),
                                                          "timeout for ACSE messages");
        cmd.addOption("--dimse-timeout",       "-td",  1, "[s]econds: integer (default: unlimited)",
                                                          "timeout for DIMSE messages");
        CONVERT_TO_STRING("[n]umber of bytes: integer (" << ASC_MINIMUMPDUSIZE << ".." << ASC_MAXIMUMPDUSIZE << ")", optString5);
        CONVERT_TO_STRING("set max receive pdu to n bytes (default: " << opt_maxPDULength << ")", optString6);
        cmd.addOption("--max-pdu",             "-pdu", 1, optString5.c_str(),
                                                          optString6.c_str());
        cmd.addOption("--disable-host-lookup", "-dhl",    "disable hostname lookup");

    /* evaluate command line */
    prepareCmdLineArgs(argc, argv, OFFIS_CONSOLE_APPLICATION);
    if (app.parseCommandLine(cmd, argc, argv))
    {
        /* check exclusive options first */
        if (cmd.hasExclusiveOption())
        {
            if (cmd.findOption("--version"))
            {
                app.printHeader(OFTrue /*print host identifier*/);
                COUT << OFendl << "External libraries used: none" << OFendl;
                return EXITCODE_NO_ERROR;
            }
        }

        /* general options */
        OFLog::configureFromCommandLine(cmd, app);
        if (cmd.findOption("--verbose-pc"))
        {
            app.checkDependence("--verbose-pc", "verbose mode", dcmrecvLogger.isEnabledFor(OFLogger::INFO_LOG_LEVEL));
            opt_showPresentationContexts = OFTrue;
        }

        cmd.beginOptionBlock();
        if (cmd.findOption("--aetitle"))
        {
            app.checkValue(cmd.getValue(opt_aeTitle));
            opt_useCalledAETitle = OFFalse;
        }
        if (cmd.findOption("--use-called-aetitle"))
            opt_useCalledAETitle = OFTrue;

        if (cmd.findOption("--commit-wait-timeout"))
            app.checkValue(cmd.getValueAndCheckMin(opt_commitWaitTimeout, 1));
        if (cmd.findOption("--peer-port")) 
            app.checkValue(cmd.getValueAndCheckMin(opt_peerPort, 104));
 
        if (cmd.findOption("--acse-timeout"))
            app.checkValue(cmd.getValueAndCheckMin(opt_acseTimeout, 1));
        if (cmd.findOption("--dimse-timeout"))
        {
            app.checkValue(cmd.getValueAndCheckMin(opt_dimseTimeout, 1));
            opt_blockingMode = DIMSE_NONBLOCKING;
        }
        if (cmd.findOption("--max-pdu"))
            app.checkValue(cmd.getValueAndCheckMinMax(opt_maxPDULength, ASC_MINIMUMPDUSIZE, ASC_MAXIMUMPDUSIZE));
        if (cmd.findOption("--disable-host-lookup"))
            opt_HostnameLookup = OFFalse;
        cmd.endOptionBlock();

      /* command line parameters */
      app.checkParam(cmd.getParamAndCheckMinMax(1, opt_port, 1, 65535));

  }

    /* print resource identifier */
    OFLOG_DEBUG(dcmrecvLogger, rcsid << OFendl);

    /* make sure data dictionary is loaded */
    if (!dcmDataDict.isDictionaryLoaded())
    {
        OFLOG_WARN(dcmrecvLogger, "no data dictionary loaded, check environment variable: "
            << DCM_DICT_ENVIRONMENT_VARIABLE);
    }

    /* start with the real work */
    DcmStorCmtSCP storcmtSCP;
    OFCondition status;

    OFLOG_INFO(dcmrecvLogger, "configuring service class provider ...");

    /* set general network parameters */
    storcmtSCP.setPort(OFstatic_cast(Uint16, opt_port));
    storcmtSCP.setPeerPort(OFstatic_cast(Uint16, opt_peerPort));
    storcmtSCP.setAETitle(opt_aeTitle);
    storcmtSCP.setMaxReceivePDULength(OFstatic_cast(Uint32, opt_maxPDULength));
    storcmtSCP.setACSETimeout(OFstatic_cast(Uint32, opt_acseTimeout));
    storcmtSCP.setDIMSETimeout(OFstatic_cast(Uint32, opt_dimseTimeout));
    storcmtSCP.setDIMSEBlockingMode(opt_blockingMode);
    storcmtSCP.setVerbosePCMode(opt_showPresentationContexts);
    storcmtSCP.setRespondWithCalledAETitle(opt_useCalledAETitle);
    storcmtSCP.setHostLookupEnabled(opt_HostnameLookup);
    storcmtSCP.setCommitWaitTimeout(opt_commitWaitTimeout);

    OFLOG_INFO(dcmrecvLogger, "starting service class provider and listening ...");

    /* start SCP and listen on the specified port */
    status = storcmtSCP.listen();
    if (status.bad())
    {
        OFLOG_FATAL(dcmrecvLogger, "cannot start SCP and listen on port " << opt_port << ": " << status.text());
        return EXITCODE_CANNOT_START_SCP_AND_LISTEN;
    }

    /* make sure that everything is cleaned up properly */
#ifdef DEBUG
    /* useful for debugging with dmalloc */
    dcmDataDict.clear();
#endif

    return EXITCODE_NO_ERROR;
}
