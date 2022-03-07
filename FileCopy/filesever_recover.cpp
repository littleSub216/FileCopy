// --------------------------------------------------------------
//
//
//
//        COMMAND LINE
//
//              fileserver <networknastiness> <filenastiness> <targetdir>
//
// --------------------------------------------------------------

#include "c150nastydgmsocket.h"
#include "c150debug.h"
#include <fstream>
#include <cstdlib>
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

using namespace std;         // for C++ std library
using namespace C150NETWORK; // for all the comp150 utilities

void setUpDebugLogging(const char *logname, int argc, char *argv[]);
void checkDirectory(char *dirname);
string checksum(string dirname, string filename); // generate checksum                   // convert sha to string
vector<string> split(string s, string delimiter); // split the incoming message
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
    ssize_t readlen;           // amount of data read from socket
    char incomingMessage[512]; // received message data
    int networknastiness;      // how aggressively do we drop packets
    int filenastiness;         //corruption during the read and write
    const int PACKETSIZE = 256;
    string response;
    size_t len; // length of w length

    string generatechecksum; // string hash goes here
    string originalchecksum; // the checksum of original file
    string filename;         // the filename recieved
    string incoming;         // recieve incoming message
    string filesize;

    vector<string> splitinput; // to split the incoming message
    string delim = "#";        //  the delimeter to spilt the incoming message
    string suffix = ".tmp";
    DIR *TARGET;
    void *buffer;
    int packetnumber; // count the total numebr of the packets
    int packetcount;  // store the seqeunce of coming packets
    vector<int> checkoreder;
    struct dirent *sourceFile; // Directory entry for source file

    //
    // Check command line and parse arguments
    //
    if (argc != 4)
    {
        fprintf(stderr, "Correct syntxt is: %s <networknastiness> <filenastiness> <targetdir>\n", argv[0]);
        exit(1);
    }
    if (strspn(argv[1], "0123456789") != strlen(argv[1]))
    {
        fprintf(stderr, "networknastiness %s is not numeric\n", argv[1]);
        fprintf(stderr, "Correct syntxt is: %s <networknastiness> <filenastiness> <targetdir>\n", argv[0]);
        exit(4);
    }
    networknastiness = atoi(argv[1]); // convert command line string to integer

    if (strspn(argv[2], "0123456789") != strlen(argv[1]))
    {
        fprintf(stderr, "filenastiness %s is not numeric\n", argv[1]);
        fprintf(stderr, "Correct syntxt is: %s <networknastiness> <filenastiness> <targetdir>\n", argv[0]);
        exit(4);
    }
    filenastiness = atoi(argv[2]); // convert command line string to integer
    //
    //  Set up debug message logging
    //
    printf("network nastiness is set to: %d\n", networknastiness);
    printf("file nastiness is set to: %d\n", filenastiness);
    setUpDebugLogging("fileserverdebug.txt", argc, argv);

    //
    // We set a debug output indent in the server only, not the client.
    // That way, if we run both programs and merge the logs this way:
    //
    //    cat pingserverdebug.txt pingserverclient.txt | sort
    //
    // it will be easy to tell the server and client entries apart.
    //
    // Note that the above trick works because at the start of each
    // log entry is a timestamp that sort will indeed arrange in
    // timestamp order, thus merging the logs by time across
    // server and client.
    //
    c150debug->setIndent("    "); // if we merge client and server
    // logs, server stuff will be indented

    //
    // Create socket, loop receiving and responding
    //
    try
    {
        //
        // Create the socket
        //
        c150debug->printf(C150APPLICATION, "Creating C150NastyDgmSocket(nastiness=%d)",
                          networknastiness);
        C150DgmSocket *sock = new C150NastyDgmSocket(networknastiness);
        c150debug->printf(C150APPLICATION, "Ready to accept messages");

        //
        // infinite loop processing messages
        //
        while (1)
        {

            //
            // Read a packet
            // -1 in size below is to leave room for null
            //
            readlen = sock->read(incomingMessage, sizeof(incomingMessage) - 1);

            if (readlen == 0)
            {
                c150debug->printf(C150APPLICATION, "Read zero length message, trying again");
                continue;
            }

            //
            // Clean up the message in case it contained junk
            //
            incomingMessage[readlen] = '\0'; // make sure null terminated
            incoming(incomingMessage);       // Convert to C++ string ...it's slightly
                                             // easier to work with, and cleanString
                                             // expects it
            cleanString(incoming);           // c150ids-supplied utility: changes
                                             // non-printing characters to .
            splitinput = split(incoming, delim);

            *GRADING << "File: " << filename.c_str() << " starting to receive file" << endl;

            c150debug->printf(C150APPLICATION, "Successfully read %d bytes. Message=\"%s\"",
                              readlen, incoming.c_str());

            //
            //  create the message to return
            //
            if (splitinput.size() == 3) // recive the request message
            {
                originalchecksum = splitinput[0];
                filename = splitinput[1] + suffix; // add the temp suffix
                filesize = atoi(splitinput[2]);
                packetnumber = filesize / PACKETSIZE;
                buffer = (void *)malloc(filesize);

                checkDirectory(argv[3]);
                //
                // Open the target directory
                //
                TARGET = opendir(argv[3]);
                if (TARGET == NULL)
                {
                    fprintf(stderr, "Error opening target directory %s \n", argv[3]);
                    exit(8);
                }
                // closedir(TARGET); // we just wanted to be sure it was there
                string targetName = makeFileName(argv[3], filename);
                //
                // Define the wrapped file descriptors
                //

                NASTYFILE outputFile(filenastiness);
                // do an fopen on the input file
                fopenretval = outputFile.fopen(filename.c_str(), "rb");
                if (fopenretval == NULL)
                {
                    cerr << "Error opening input file " << sourceName << " errno=" << strerror(errno) << endl;
                    exit(12);
                }
                response = argv[3]; // return the target directory for confirmation
            }
            else if (checkoreder.size() < packetnumber)
            {
                printf("the data is %s\n", memsplitinput[0]);

                // data pakcet
                // recieve the file by packet
                char *curr_string = (char *)((unsigned long)buffer + memsplitinput[1] * PACKETSIZE); // set the offset in the buffer string

                //
                // sourceSize = strlen(incoming[0]);
                //
                // write the packet
                len = outputFile.fwrite(curr_string, 1, PACKETSIZE, memsplitinput[0]);
                if (len != PACKETSIZE)
                {
                    cerr << "Error writing packet " << memsplitinput[1] << "  errno=" << strerror(errno) << endl;
                    exit(16);
                }
                // send the ack?
                
                checkoreder.push_back(atoi(memsplitinput[1]));
            }
            // go through th value in checkorder to ask for resend
            else
            {
                //
                // Free the input buffer to avoid memory leaks after the file is wrote
                //
                free(buffer);
                // begin  end to end check
                // *GRADING << "File: " << filename.c_str() << " received, beginning end-to-end check" << endl;

                // generate the sha code for tmp the tmp file
                generatechecksum = checksum(argv[3], (char *)filename);
                printf("Checked file is: %s\n", filename.c_str());
                printf("Generate checksum is: %s\n", originalchecksum.c_str());

                if (generatechecksum.compare(originalchecksum) == 0)
                {
                    response = "Success";
                    *GRADING << "File: " << filename.c_str() << " end-to-end check succeeded " << endl;
                    filename = filename.erase(filename.end() - 4, filename.end()); // remove the suffix
                    break;
                }
                else
                {
                    if (remove(filename) != 0)
                        perror("Error deleting file");
                    else
                        response = "Fail";
                    *GRADING << "File: " << filename.c_str() << "end-to-end check failed " << endl;
                }
                
            }
        }
        //
        // write the return message
        //
        c150debug->printf(C150APPLICATION, "Responding with message=\"%s\"",
                          response.c_str());
        sock->write(response.c_str(), response.length() + 1);
    }
}

catch (C150NetworkException &e)
{
    // Write to debug log
    c150debug->printf(C150ALWAYSLOG, "Caught C150NetworkException: %s\n",
                      e.formattedExplanation().c_str());
    // In case we're logging to a file, write to the console too
    cerr << argv[0] << ": caught C150NetworkException: " << e.formattedExplanation() << endl;
}

// This only executes if there was an error caught above
return 4;
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
//        NEEDSWORK: should be factored and shared w/pingclient
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
    // debug output to a file. Comment them to
    // default to the console
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
// ------------------------------------------------------
//
//                   string tools
//
// modified the strings
//
// ------------------------------------------------------
vector<string> split(string s, string delimiter)
{
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    string token;
    vector<string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != string::npos)
    {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }
    res.push_back(s.substr(pos_start));
    return res;
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