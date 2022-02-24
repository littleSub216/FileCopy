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

using namespace std;         // for C++ std library
using namespace C150NETWORK; // for all the comp150 utilities

// forward declarations
void checkAndPrintMessage(ssize_t readlen, char *buf, ssize_t bufferlen);
void setUpDebugLogging(const char *logname, int argc, char *argv[]);
// file copy declarations
void copyFile(string sourceDir, string fileName, string targetDir, int nastiness); // fwd decl
bool isFile(string fname);
void checkDirectory(char *dirname);
void checksum(char filename[], unsigned char shaComputedHash[]); // generate checksum
string convertToString(unsigned char *a);

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

    //
    // Variable declarations
    //
    ssize_t readlen;           // amount of data read from socket
    char incomingMessage[512]; // received message data
    int networknastiness;      // how aggressively do we drop packets
    int filenastiness;         // corruption during the read and write

    unsigned char shaComputedHash[20]; // hash goes here
    string originalchecksum;
    string requestCheck; // send the  checksum
    int retry = 0;

    DIR *SRC;                  // Unix descriptor for open directory
    DIR *TARGET;               // Unix descriptor for target
    struct dirent *sourceFile; // Directory entry for source file

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

        // Send the message to the server
        c150debug->printf(C150APPLICATION, "%s: Request to copy file from directory: \"%s\"",
                          argv[0], argv[msgArg]);
        sock->write(argv[msgArg], strlen(argv[msgArg]) + 1); // +1 includes the null

        // Read the targetname from the server
        c150debug->printf(C150APPLICATION, "%s: Returned from write, doing read()",
                          argv[0]);
        readlen = sock->read(incomingMessage, sizeof(incomingMessage));
        // if (readlen == 0)
        // {
        //     c150debug->printf(C150APPLICATION, "Read zero length message, trying again");
        //     continue;
        // }
        //
        // Clean up the message in case it contained junk
        //
        incomingMessage[readlen] = '\0';  // make sure null terminated
        string incoming(incomingMessage); // Convert to C++ string ...it's slightly
                                          // easier to work with, and cleanString
                                          // expects it
        cleanString(incoming);            // c150ids-supplied utility: changes
                                          // non-printing characters to .
        c150debug->printf(C150APPLICATION, "Successfully read %d bytes. Message=\"%s\"",
                          readlen, incoming.c_str());

        // Check and print the incoming message
        checkAndPrintMessage(readlen, incomingMessage, sizeof(incomingMessage));
        // check the directory or the end to end result
        if (incoming.compare("TARGET") == 0)
        {
            checkDirectory(argv[4]);
            checkDirectory(incomingMessage);
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
            // Open the target directory (we don't really need to do
            // this here, but it will give cleaner error messages than
            // waiting for individual file writes to fail.
            //
            TARGET = opendir(incoming.c_str());
            if (TARGET == NULL)
            {
                fprintf(stderr, "Error opening target directory %s \n", incoming.c_str());
                exit(8);
            }
            closedir(TARGET); // we just wanted to be sure it was there
                              // SRC dir remains open for loop below

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

                // do the copy -- this will check for and
                // skip subdirectories
                copyFile(argv[4], sourceFile->d_name, incoming.c_str(), filenastiness);
                // generate the sha code for inputfile
                checksum((char *)sourceFile->d_name, shaComputedHash);
                originalchecksum = convertToString(shaComputedHash);
                string filename(sourceFile->d_name);
                requestCheck = filename + "#" + originalchecksum;
                sock->write(requestCheck.c_str(), strlen(requestCheck.c_str()) + 1);
                readlen = sock->read(incomingMessage, sizeof(incomingMessage));
                printf("Original Checksum is:\n ", originalchecksum);
                if (readlen == 0)
                {
                    c150debug->printf(C150APPLICATION, "Read zero length message, trying again");
                    retry++;
                    continue;
                }
                //
                // Clean up the message in case it contained junk
                //
                incomingMessage[readlen] = '\0';  // make sure null terminated
                string incoming(incomingMessage); // Convert to C++ string ...it's slightly
                                                  // easier to work with, and cleanString
                                                  // expects it
                cleanString(incoming);            // c150ids-supplied utility: changes
                                                  // non-printing characters to .
                if (incoming.compare("Success") == 0)
                {
                    break;
                }
                else if (retry < 5)
                {
                    retry++;
                }
                else
                {
                    printf("Fail 5 times");
                    exit(0);
                }
            }
            closedir(SRC);
        }
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
//                   makeFileName
//
// Put together a directory and a file name, making
// sure there's a / in between
//
// ------------------------------------------------------

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
//                   copyFile
//
// Copy a single file from sourcdir to target dir
//
// ------------------------------------------------------

void copyFile(string sourceDir, string fileName, string targetDir, int nastiness)
{

    //
    //  Misc variables, mostly for return codes
    //
    void *fopenretval;
    size_t len;
    string errorString;
    char *buffer;
    struct stat statbuf;
    size_t sourceSize;

    //
    // Put together directory and filenames SRC/file TARGET/file
    //
    string sourceName = makeFileName(sourceDir, fileName);
    string targetName = makeFileName(targetDir, fileName);

    //
    // make sure the file we're copying is not a directory
    //
    if (!isFile(sourceName))
    {
        cerr << "Input file " << sourceName << " is a directory or other non-regular file. Skipping" << endl;
        return;
    }

    cout << "Copying " << sourceName << " to " << targetName << endl;

    // - - - - - - - - - - - - - - - - - - - - -
    // LOOK HERE! This demonstrates the
    // COMP 150 Nasty File interface
    // - - - - - - - - - - - - - - - - - - - - -

    try
    {

        //
        // Read whole input file
        //
        if (lstat(sourceName.c_str(), &statbuf) != 0)
        {
            fprintf(stderr, "copyFile: Error stating supplied source file %s\n", sourceName.c_str());
            exit(20);
        }

        //
        // Make an input buffer large enough for
        // the whole file
        //
        sourceSize = statbuf.st_size;
        buffer = (char *)malloc(sourceSize);

        //
        // Define the wrapped file descriptors
        //
        // All the operations on outputFile are the same
        // ones you get documented by doing "man 3 fread", etc.
        // except that the file descriptor arguments must
        // be left off.
        //
        // Note: the NASTYFILE type is meant to be similar
        //       to the Unix FILE type
        //
        NASTYFILE inputFile(nastiness);  // See c150nastyfile.h for interface
        NASTYFILE outputFile(nastiness); // NASTYFILE is supposed to
                                         // remind you of FILE
                                         //  It's defined as:
                                         // typedef C150NastyFile NASTYFILE

        // do an fopen on the input file
        fopenretval = inputFile.fopen(sourceName.c_str(), "rb");
        // wraps Unix fopen
        // Note rb gives "read, binary"
        // which avoids line end munging

        if (fopenretval == NULL)
        {
            cerr << "Error opening input file " << sourceName << " errno=" << strerror(errno) << endl;
            exit(12);
        }

        //
        // Read the whole file
        //
        len = inputFile.fread(buffer, 1, sourceSize);
        if (len != sourceSize)
        {
            cerr << "Error reading file " << sourceName << "  errno=" << strerror(errno) << endl;
            exit(16);
        }

        if (inputFile.fclose() != 0)
        {
            cerr << "Error closing input file " << targetName << " errno=" << strerror(errno) << endl;
            exit(16);
        }

        //
        // do an fopen on the output file
        //
        fopenretval = outputFile.fopen(targetName.c_str(), "wb");
        // wraps Unix fopen
        // Note wb gives "write, binary"
        // which avoids line and munging

        //
        // Write the whole file
        //
        len = outputFile.fwrite(buffer, 1, sourceSize);
        if (len != sourceSize)
        {
            cerr << "Error writing file " << targetName << "  errno=" << strerror(errno) << endl;
            exit(16);
        }

        if (outputFile.fclose() == 0)
        {
            cout << "Finished writing file " << targetName << endl;
        }
        else
        {
            cerr << "Error closing output file " << targetName << " errno=" << strerror(errno) << endl;
            exit(16);
        }

        //
        // Free the input buffer to avoid memory leaks
        //
        free(buffer);

        //
        // Handle any errors thrown by the file framekwork
        //
    }
    catch (C150Exception &e)
    {
        cerr << "nastyfiletest:copyfile(): Caught C150Exception: " << e.formattedExplanation() << endl;
    }
}

// ------------------------------------------------------
//
//                   checksum
//
// Generate the SHA based on the input files
//
// ------------------------------------------------------
void checksum(char filename[], unsigned char shaComputedHash[])
{
    int i;
    ifstream *t;
    stringstream *buffer;
    unsigned char obuf[20];

    t = new ifstream(filename);
    buffer = new stringstream;
    *buffer << t->rdbuf();
    SHA1((const unsigned char *)buffer->str().c_str(),
         (buffer->str()).length(), obuf);
    for (i = 0; i < 20; i++)
    {
        shaComputedHash[i] = (unsigned int)obuf[i];
    }
    delete t;
    delete buffer;
}

string convertToString(unsigned char *a)
{
    string s(reinterpret_cast<char *>(a));

    return s;
}