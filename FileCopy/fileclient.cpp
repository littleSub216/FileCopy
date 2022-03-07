// --------------------------------------------------------------
//
//
//
//        COMMAND LINE
//
//              fileclient <server> <networknastiness> <filenastiness> <srcdir>
//
//
// ------------------------#include "c150nastydgmsocket.h"
#include "c150nastydgmsocket.h"
#include "c150dgmsocket.h"
#include "c150debug.h"
#include <fstream>
#include "c150nastyfile.h" // for c150nastyfile & framework
#include "c150grading.h"
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring> // for errno string formatting
#include <cerrno>
#include <cstring>  // for strerro
#include <iostream> // for cout
#include <fstream>  // for input files
#include <openssl/sha.h>
#include <vector>
// #define PACKETSIZE 256
// #define delim "#"

using namespace std;         // for C++ std library
using namespace C150NETWORK; // for all the comp150 utilities

// forward declarations
void checkAndPrintMessage(ssize_t readlen, char *buf, ssize_t bufferlen);
void setUpDebugLogging(const char *logname, int argc, char *argv[]);
// file copy declarations
void copyFile(string sourceDir, string fileName, string targetDir, int nastiness, int count); // fwd decl
bool isFile(string fname);
void checkDirectory(char *dirname);
string checksum(string dirname, string filename); // generate checksum
string makeFileName(string dir, string name);
void writePacket(string filename, int packernumber, int lastPacketLen, C150DgmSocket *sock, int fileNastiness);
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                    Command line arguments
//
// The following are used as subscripts to argv, the command line arguments
// If we want to change the command line syntax, doing this
// symbolically makes it a bit easier.
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

const int serverArg = 1; // server name is 1st arg
const int msgArg = 4;    // src directory is 4th arg

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                           main program
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int main(int argc, char *argv[])
{
    GRADEME(argc, argv);

    //
    // Variable declarations
    //
    const int PACKETSIZE = 256;
    string delim = "#"; //  the delimeter to spilt the incoming message

    // ssize_t readlen;           // amount of data read from socket
    char incomingMessage[512]; // received message data
    int networknastiness;      // how aggressively do we drop packets
    int filenastiness;         // corruption during the read and write

    // unsigned char shaComputedHash[20]; // hash goes here
    string originalchecksum;
    // string filelen;
    string requestCheck; // send the  checksum
    int retry = 0;

    DIR *SRC;                  // Unix descriptor for open directory
    struct dirent *sourceFile; // Directory entry for source file
    struct stat statbuf;       // store the whole file and return the size

    vector<string> splitfile; // to split file into packets
    string errorString;

    size_t sourceSize;
    int packetNumber;
    int lastPacketLen;
    //
    // Make sure command line looks right
    //
    if (argc != 5)
    {
        fprintf(stderr, "Correct syntxt is: %s <server> <networknastiness> <filenastiness> <srcdir>\n", argv[0]);
        exit(1);
    }
    if (strspn(argv[2], "0123456789") != strlen(argv[2]))
    {
        fprintf(stderr, "networknastiness %s is not numeric\n", argv[2]);
        fprintf(stderr, "Correct syntxt is: %s <networknastiness> <filenastiness> <targetdir>\n", argv[0]);
        exit(4);
    }
    networknastiness = atoi(argv[2]); // convert command line string to integer

    if (strspn(argv[3], "0123456789") != strlen(argv[3]))
    {
        fprintf(stderr, "filenastiness %s is not numeric\n", argv[3]);
        fprintf(stderr, "Correct syntxt is: %s <networknastiness> <filenastiness> <targetdir>\n", argv[0]);
        exit(4);
    }
    filenastiness = atoi(argv[3]); // convert command line string to integer

    //
    //  Set up debug message logging
    //
    setUpDebugLogging("fileclientdebug.txt", argc, argv);

    //
    //
    //        Send / receive / copy
    //
    try
    {

        c150debug->printf(C150APPLICATION, "Creating C150NastyDgmSocket(nastiness=%d)",
                          networknastiness);
        C150DgmSocket *sock = new C150NastyDgmSocket(networknastiness);
        c150debug->printf(C150APPLICATION, "Ready to accept messages");

        // Tell the DGMSocket which server to talk to
        sock->setServerName(argv[serverArg]);
        // set timeout to be 3 seconds
        sock->turnOnTimeouts(3000);

        checkDirectory(argv[4]);
        //
        // Open the source directory
        //
        SRC = opendir(argv[4]);
        if (SRC == NULL)
        {
            fprintf(stderr, "Error opening source directory %s\n", argv[4]);
            exit(8);
        }
        //
        //  Loop copying the files
        //
        //  copyfile takes name of target file
        //
        while ((sourceFile = readdir(SRC)) != NULL)
        {
            retry = 0;
            // skip the . and .. names
            if ((strcmp(sourceFile->d_name, ".") == 0) ||
                (strcmp(sourceFile->d_name, "..") == 0))
                continue; // never copy . or ..

            string sourceName = makeFileName(argv[4], sourceFile->d_name); // make the absolute path of file
                                                                           //
            // make sure the file we're copying is not a directory
            //
            if (!isFile(sourceName))
            {
                cerr << "Input file " << sourceName << " is a directory or other non-regular file. Skipping" << endl;
                exit(8);
            }

            cout << "Copying " << sourceName << endl;
            *GRADING << "File: " << sourceName << ", beginning transmission, attempt " << endl;

            //
            // Read whole input file to get the total size
            //
            if (lstat(sourceName.c_str(), &statbuf) != 0)
            {
                fprintf(stderr, "copyFile: Error stating supplied source file %s\n", sourceName.c_str());
                exit(20);
            }
            sourceSize = statbuf.st_size; // the size of the whole file
            packetNumber = sourceSize / PACKETSIZE;
            lastPacketLen = sourceSize % PACKETSIZE;
            if (lastPacketLen != 0)
            {
                packetNumber++;
            }

            // send the filename checksom and file length
            originalchecksum = checksum(argv[4], (char *)sourceFile->d_name);
            // printf("Original checksum is: %s\n", originalchecksum.c_str());
            string filename(sourceFile->d_name);

            requestCheck = originalchecksum + delim + filename + delim + to_string(sourceSize);
            // printf("Original requestCheck is: %s\n", requestCheck.c_str());
            sock->write(requestCheck.c_str(), strlen(requestCheck.c_str()) + 1); // send the first head info

            while (retry < 5)
            {
                /// SPLIT AND SEND THE PACKET
                writePacket(sourceName, packetNumber, lastPacketLen, sock, filenastiness);
                sock->read(incomingMessage, sizeof(incomingMessage));
                if (strcmp(incomingMessage, "Success") == 0)
                {
                    printf("end to end check is: %s\n", incomingMessage);
                    printf("========================================\n");
                    break;
                }
                retry++;
            }
            if (retry < 5)
            {
                *GRADING << "File: " << sourceName.c_str() << " transmission complete " << endl;
            }
            else
            {

                printf("lose connection to the server\n");
                exit(1);
            }
        }
        closedir(SRC);
    }
    //
    //  Handle networking errors -- for now, just print message and give up!
    //
    catch (C150NetworkException &e)
    {
        // Write to debug log
        c150debug->printf(C150ALWAYSLOG, "Caught C150NetworkException: %s\n",
                          e.formattedExplanation().c_str());
        // In case we're logging to a file, write to the console too
        cerr << argv[0] << ": caught C150NetworkException: " << e.formattedExplanation()
             << endl;
    }

    return 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                     checkAndPrintMessage
//
//        Make sure length is OK, clean up response buffer
//        and print it to standard output.
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void checkAndPrintMessage(ssize_t readlen, char *msg, ssize_t bufferlen)
{
    //
    // Except in case of timeouts, we're not expecting
    // a zero length read
    //
    if (readlen == 0)
    {
        throw C150NetworkException("Unexpected zero length read in client");
    }

    // DEFENSIVE PROGRAMMING: we aren't even trying to read this much
    // We're just being extra careful to check this
    if (readlen > (int)(bufferlen))
    {
        throw C150NetworkException("Unexpected over length read in client");
    }

    //
    // Make sure server followed the rules and
    // sent a null-terminated string (well, we could
    // check that it's all legal characters, but
    // at least we look for the null)
    //
    if (msg[readlen - 1] != '\0')
    {
        throw C150NetworkException("Client received message that was not null terminated");
    };

    //
    // Use a routine provided in c150utility.cpp to change any control
    // or non-printing characters to "." (this is just defensive programming:
    // if the server maliciously or inadvertently sent us junk characters, then we
    // won't send them to our terminal -- some
    // control characters can do nasty things!)
    //
    // Note: cleanString wants a C++ string, not a char*, so we make a temporary one
    // here. Not super-fast, but this is just a demo program.
    string s(msg);
    cleanString(s);

    // Echo the response on the console

    c150debug->printf(C150APPLICATION, "PRINTING RESPONSE: Response received is \"%s\"\n", s.c_str());
    printf("Response received is \"%s\"\n", s.c_str());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                     setUpDebugLogging
//
//        For COMP 150-IDS, a set of standards utilities
//        are provided for logging timestamped debug messages.
//        You can use them to write your own messages, but
//        more importantly, the communication libraries provided
//        to you will write into the same logs.
//
//        As shown below, you can use the enableLogging
//        method to choose which classes of messages will show up:
//        You may want to turn on a lot for some debugging, then
//        turn off some when it gets too noisy and your core code is
//        working. You can also make up and use your own flags
//        to create different classes of debug output within your
//        application code
//
//        NEEDSWORK: should be factored into shared code w/pingserver
//        NEEDSWORK: document arguments
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void setUpDebugLogging(const char *logname, int argc, char *argv[])
{

    //
    //           Choose where debug output should go
    //
    // The default is that debug output goes to cerr.
    //
    // Uncomment the following three lines to direct
    // debug output to a file. Comment them
    // to default to the console.
    //
    // Note: the new DebugStream and ofstream MUST live after we return
    // from setUpDebugLogging, so we have to allocate
    // them dynamically.
    //
    //
    // Explanation:
    //
    //     The first line is ordinary C++ to open a file
    //     as an output stream.
    //
    //     The second line wraps that will all the services
    //     of a comp 150-IDS debug stream, and names that filestreamp.
    //
    //     The third line replaces the global variable c150debug
    //     and sets it to point to the new debugstream. Since c150debug
    //     is what all the c150 debug routines use to find the debug stream,
    //     you've now effectively overridden the default.
    //
    ofstream *outstreamp = new ofstream(logname);
    DebugStream *filestreamp = new DebugStream(outstreamp);
    DebugStream::setDefaultLogger(filestreamp);

    //
    //  Put the program name and a timestamp on each line of the debug log.
    //
    c150debug->setPrefix(argv[0]);
    c150debug->enableTimestamp();

    //
    // Ask to receive all classes of debug message
    //
    // See c150debug.h for other classes you can enable. To get more than
    // one class, you can or (|) the flags together and pass the combined
    // mask to c150debug -> enableLogging
    //
    // By the way, the default is to disable all output except for
    // messages written with the C150ALWAYSLOG flag. Those are typically
    // used only for things like fatal errors. So, the default is
    // for the system to run quietly without producing debug output.
    //
    c150debug->enableLogging(C150APPLICATION | C150NETWORKTRAFFIC |
                             C150NETWORKDELIVERY);
}

// ------------------------------------------------------
//
//                   checkDirectory
//
//  Make sure directory exists
//
// ------------------------------------------------------

void checkDirectory(char *dirname)
{
    struct stat statbuf;
    if (lstat(dirname, &statbuf) != 0)
    {
        fprintf(stderr, "Error stating supplied source directory %s\n", dirname);
        exit(8);
    }

    if (!S_ISDIR(statbuf.st_mode))
    {
        fprintf(stderr, "File %s exists but is not a directory\n", dirname);
        exit(8);
    }
}

// ------------------------------------------------------
//
//                   isFile
//
//  Make sure the supplied file is not a directory or
//  other non-regular file.
//
// ------------------------------------------------------

bool isFile(string fname)
{
    const char *filename = fname.c_str();
    struct stat statbuf;
    if (lstat(filename, &statbuf) != 0)
    {
        fprintf(stderr, "isFile: Error stating supplied source file %s\n", filename);
        return false;
    }

    if (!S_ISREG(statbuf.st_mode))
    {
        fprintf(stderr, "isFile: %s exists but is not a regular file\n", filename);
        return false;
    }
    return true;
}

// ------------------------------------------------------
//
//                   checksum
//
// Generate the SHA based on the input files
//
// ------------------------------------------------------
string checksum(string dirname, string filename)
{
    int i;
    ifstream *t;
    stringstream *buffer;
    unsigned char obuf[20];
    char stringbuffer[50];
    string absolute = dirname + "/" + filename;
    string checksum;

    // printf("SHA1 (\"%s\") = ",absolute.c_str());

    t = new ifstream(absolute);
    buffer = new stringstream;
    *buffer << t->rdbuf();
    SHA1((const unsigned char *)buffer->str().c_str(),
         (buffer->str()).length(), obuf);

    for (i = 0; i < 20; i++)
    {
        sprintf(stringbuffer, "%02x", (unsigned int)obuf[i]);
        string tmp(stringbuffer);
        checksum += tmp;
    }
    // printf("checksum.c_str());

    delete t;
    delete buffer;
    return checksum;
}

string
makeFileName(string dir, string name)
{
    stringstream ss;

    ss << dir;
    // make sure dir name ends in /
    if (dir.substr(dir.length() - 1, 1) != "/")
        ss << '/';
    ss << name;      // append file name to dir
    return ss.str(); // return dir/name
}

void writePacket(string filename, int packernumber, int lastPacketLen, C150DgmSocket *sock, int filenastiness)
{
    char incomingMessage[512];
    char *buffer;
    const int PACKETSIZE = 256;
    string delim = "#";
    void *fopenretval;
    int retry;

    NASTYFILE inputFile(filenastiness); // See c150nastyfile.h for interface
                                        // do an fopen on the input file
    fopenretval = inputFile.fopen(filename.c_str(), "rb");
    if (fopenretval == NULL)
    {
        cerr << "Error opening input file " << filename << " errno =" << strerror(errno) << endl;
        exit(12);
    }
    buffer = (char *)malloc(PACKETSIZE);

    for (int i = 0; i < packernumber - 1; i++)
    {
        retry = 0;
        while (retry < 4)
        {

            inputFile.fread(buffer, 1, PACKETSIZE);
            buffer[PACKETSIZE] = '\0';
            string message(buffer);

            message = message + delim + to_string(i);
            sock->write(message.c_str(), strlen(message.c_str()) + 1); // +1 includes the null
            // printf("Packet %d is sent\n ", i);
            sock->read(incomingMessage, sizeof(incomingMessage));
            printf("IncomingMessage for packet %d  is %s \n", i, incomingMessage);
            if (strcmp(incomingMessage, "Recived") == 0)
            {
                break;
            }
            retry++;
        }
    }
    // last packet
    int last;
    if (lastPacketLen == 0)
    {
        last = 256;
    }
    else
    {
        last = lastPacketLen;
    }
    inputFile.fread(buffer, 1, last);
    sock->write(buffer, strlen(buffer) + 1);
    free(buffer);
}