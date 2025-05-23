/*
 * This file is released under the terms of the Artistic License.  Please see
 * the file LICENSE, included in this package, for details.
 *
 * Copyright The DBT-5 Authors
 *
 * executable that opens the Brokerage House to business
 * 25 July 2006
 */

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include "BrokerageHouse.h"
#include "DBT5Consts.h"

// Establish defaults for command line option
int iClientSide = 0;
int iListenPort = iBrokerageHousePort;
bool verbose = false;

char szHost[iMaxHostname + 1] = "";
char szDBName[iMaxDBName + 1] = "";
char szDBPort[iMaxPort + 1] = "";
char szMEEHost[iMaxHostname + 1] = "localhost";
char szMEEPort[iMaxPort + 1] = "";
char outputDirectory[iMaxPath + 1] = ".";

// shows program usage
void
usage()
{
	cout << "Usage: BrokerageHouseMain [options]" << endl << endl;
	cout << "   Option      Default    Description" << endl;
	cout << "   =========   =========  ===============" << endl;
	cout << "   -1                     Use client-side app logic" << endl;
	cout << "   -d string              Database name" << endl;
	cout << "   -h string   localhost  Database server" << endl;
	printf("   -l integer  %-9d  Socket listen port\n", iListenPort);
	printf("   -m string   %9s  Market Exchange Emulator hostname\n",
			szMEEHost);
	printf("   -M integer  %9s  Market Exchange Emulator port\n", szMEEPort);
	cout << "   -o string   .          Output directory" << endl;
	cout << "   -p integer             Database port" << endl;
	cout << "   -v                     Verbose output" << endl;
	cout << endl;
}

// Parse command line
void
parse_command_line(int argc, char *argv[])
{
	int arg;
	char *sp;
	char *vp;

	// Scan the command line arguments
	for (arg = 1; arg < argc; ++arg) {

		// Look for a switch
		sp = argv[arg];
		if (*sp == '-') {
			++sp;
		}

		/*
		 *  Find the switch's argument.  It is either immediately after the
		 *  switch or in the next argv
		 */
		vp = sp + 1;
		// Allow for switched that don't have any parameters.
		// Need to check that the next argument is in fact a parameter
		// and not the next switch that starts with '-'.
		//
		if ((*vp == 0) && ((arg + 1) < argc) && (argv[arg + 1][0] != '-')) {
			vp = argv[++arg];
		}

		// Parse the switch
		switch (*sp) {
		case '1':
			iClientSide = 1;
			break;
		case 'd': // Database name.
			strncpy(szDBName, vp, iMaxDBName);
			szDBName[iMaxDBName] = '\0';
			break;
		case 'h': // Database host name.
			strncpy(szHost, vp, iMaxHostname);
			szHost[iMaxHostname] = '\0';
			break;
		case 'm':
			strncpy(szMEEHost, vp, iMaxHostname);
			szMEEHost[iMaxHostname] = '\0';
			break;
		case 'M': // Postmaster port
			strncpy(szMEEPort, vp, iMaxPort);
			szMEEPort[iMaxPort] = '\0';
			break;
		case 'o': // output directory
			strncpy(outputDirectory, vp, iMaxPath);
			outputDirectory[iMaxPath] = '\0';
			break;
		case 'p': // Postmaster port
			strncpy(szDBPort, vp, iMaxPort);
			szDBPort[iMaxPort] = '\0';
			break;
		case 'l':
			iListenPort = atoi(vp);
			break;
		case 'v':
			verbose = true;
			break;
		default:
			usage();
			cout << endl << "Error: Unrecognized option: " << sp << endl;
			exit(1);
		}
	}
}

int
main(int argc, char *argv[])
{
	snprintf(szMEEPort, iMaxPort, "%d", iMarketExchangePort);

	cout << "dbt5 - Brokerage House" << endl;
	cout << "Listening on port: " << iListenPort << endl << endl;

	// Parse command line
	parse_command_line(argc, argv);

	char *pidFilename = new char[1024];
	snprintf(pidFilename, 1023, "%s/bh.pid", outputDirectory);
	FILE *fpid = fopen(pidFilename, "w");
	if (fpid == NULL) {
		cerr << "ERROR: can't create pid file: " << pidFilename << endl;
		return 1;
	}
	fprintf(fpid, "%d", getpid());
	fclose(fpid);
	delete[] pidFilename;

	// Let the user know what settings will be used.
	cout << "Using the following database settings:" << endl
		 << "  Database hostname: " << szHost << endl
		 << "  Database port: " << szDBPort << endl
		 << "  Database name: " << szDBName << endl;

	cout << "Using the following Market Exchange Emulator settings:" << endl
		 << "  Hostname: " << szMEEHost << endl
		 << "  Port: " << szMEEPort << endl;
	
	// 初始化异步线程池
	spdlog::init_thread_pool(8192, 1);

	// 定义日志文件的目标目录和文件名
	string capture_dir = "/data/hwd/pg14.17/tpce-log/";
	string file_name = "capture.log";
	string full_capture_path = capture_dir + file_name;

	try {
		auto logger = spdlog::rotating_logger_mt("file_logger", full_capture_path, 
				1048576 * 10, 64);
		
		logger->set_pattern("%v");
		spdlog::set_default_logger(logger);

	} catch (const spdlog::spdlog_ex &ex) {
		cout << "Log initialization failed: " << ex.what() << endl;
        return (1);
	}

	CBrokerageHouse BrokerageHouse(szHost, szDBName, szDBPort, szMEEHost,
			szMEEPort, iListenPort, outputDirectory, iClientSide, verbose);
	cout << "Brokerage House opened for business, waiting for traders..."
		 << endl;
	try {
		BrokerageHouse.startListener();
	} catch (CBaseErr *pErr) {
		cout << "Error " << pErr->ErrorNum() << ": " << pErr->ErrorText();
		if (pErr->ErrorLoc()) {
			cout << " at " << pErr->ErrorLoc();
		}
		cout << endl;
		return 1;
	} catch (std::bad_alloc const &) {
		// operator new will throw std::bad_alloc exception if there is not
		// sufficient memory for the request.
		cout << "*** Out of memory ***" << endl;
		return 2;
	} catch (const exception &e) {
		cout << e.what() << endl;
		return 3;
	}

	pthread_exit(NULL);

	spdlog::shutdown();

	cout << "Brokerage House closed for business" << endl;
	return (0);
}
