
#include "mppsscp.h"
#include "dcmtk/ofstd/ofstd.h"
#include <dcmtk/dcmnet/assoc.h>

#define OFFIS_CONSOLE_APPLICATION "mppsscp"

#define APPLICATIONTITLE "MPPSSCP"     /* our application entity title */

#define SHORTCOL 4
#define LONGCOL 21

OFCmdUnsignedInt   opt_port = 0;
const char *       opt_respondingAETitle = APPLICATIONTITLE;

int main(int argc, char *argv[])
{
    OFConsoleApplication app(OFFIS_CONSOLE_APPLICATION, "DICOM mpps (N-CREATE,N-SET) SCP");
    OFCommandLine cmd;

    cmd.setParamColumn(LONGCOL + SHORTCOL + 4);
    cmd.addParam("port", "tcp/ip port number to listen on", OFCmdParam::PM_Optional);

    cmd.setOptionColumns(LONGCOL, SHORTCOL);
    cmd.addGroup("general options:", LONGCOL, SHORTCOL + 2);
    cmd.addOption("--help",                     "-h",      "print this help text and exit", OFCommandLine::AF_Exclusive);
    cmd.addOption("--version",                             "print version information and exit", OFCommandLine::AF_Exclusive);

    OFString opt1 = "set my AE title (default: ";
    opt1 += APPLICATIONTITLE;
    opt1 += ")";
    cmd.addOption("--aetitle",                "-aet", 1, "[a]etitle: string", opt1.c_str());
    OFLog::addOptions(cmd);

    /* evaluate command line */
    prepareCmdLineArgs(argc, argv, OFFIS_CONSOLE_APPLICATION);
    if (app.parseCommandLine(cmd, argc, argv, OFCommandLine::PF_ExpandWildcards))
    {
        /* print help text and exit */
        if (cmd.getArgCount() == 0)
            app.printUsage();

        if (cmd.findOption("--aetitle")) app.checkValue(cmd.getValue(opt_respondingAETitle));

        app.checkParam(cmd.getParamAndCheckMinMax(1, opt_port, 1, 65535));
        OFLog::configureFromCommandLine(cmd, app);

    }

    OFString config = "mppsscp.cfg";

    DcmMppsSCP *mppsscp = new DcmMppsSCP();
    mppsscp->setPort(opt_port);
    mppsscp->setAETitle(opt_respondingAETitle);
   
   
    OFCondition cond = EC_Normal;
    cond = mppsscp->loadAssociationCfgFile(config);
    if (cond.bad()) {
        printf ("loadAssociationCfgFile fail. setup manually\n");
        exit(-1);
    }

    mppsscp->listen();

    delete mppsscp;
}
