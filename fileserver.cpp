
//    Command line:
//
//         fileserver <networknastiness> <filenastiness> <targetdir>

#include "c150nastydgmsocket.h"
#include "c150nastyfile.h" // for c150nastyfile & framework
#include "c150grading.h"
#include "c150debug.h"
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

const int TargetDir = 3; // target directory name is 3th arg

void checksum(char filename[], unsigned char shaComputedHash[]);
// void copyFile(string sourceDir, string fileName, string targetDir, int nastiness); // fwd decl
bool isFile(string fname);
void checkDirectory(char *dirname);
void setUpDebugLogging(const char *logname, int argc, char *argv[]);
string convertToString(unsigned char* a);


int main(int argc, char *argv[])
{
    ssize_t readlen;           // amount of data read from socket
    char incomingMessage[512]; // received message data
    int filenastiness;         // packet drop
    int networknastiness;      // corruption on files
    struct dirent *sourceFile; // Directory entry for source file
    
    unsigned char shaComputedHash[20]; // hash goes here
    // bool end2endCheck;                 // if the file is identical with the original one
    const char delim = '#';      //  the delimeter to spilt the incoming message
    DIR *TARGET;      
               // Unix descriptor for target
    string response;
    string generatechecksum;

    // grade assignment
    GRADEME(argc, argv);

    //
    // Check command line and parse arguments
    //
    if (argc != 3)
    {
        fprintf(stderr, "Correct syntxt is: %s<networknastiness> <filenastiness> <targetdir>\n", argv[0]);
        exit(1);
    }

    if (strspn(argv[1], "0123456789") != strlen(argv[1]))
    {
        fprintf(stderr, "NetworkNastiness %s is not numeric\n", argv[1]);
        fprintf(stderr, "Correct syntxt is: %s <networknastiness> <filenastiness> <targetdir>\n", argv[0]);
        exit(4);
    }

    if (strspn(argv[2], "0123456789") != strlen(argv[2]))
    {
        fprintf(stderr, "FileNastiness %s is not numeric\n", argv[1]);
        fprintf(stderr, "Correct syntxt is: %s <networknastiness> <filenastiness> <targetdir>\n", argv[0]);
        exit(4);
    }

    filenastiness = atoi(argv[2]); // convert command line string to integer
    networknastiness = atoi(argv[1]);

    //
    //  Set up debug message logging
    //
    setUpDebugLogging("serverdebug.txt", argc, argv);
    c150debug->setIndent("    "); // if we merge client and server
    // logs, server stuff will be indented

    //
    // Create socket, loop receiving and responding
    //
    try
    {
        // Create the socket
        //
        c150debug->printf(C150APPLICATION, "Creating C150NastyDgmSocket(nastiness=%d)",
                          networknastiness);
        C150DgmSocket *sock = new C150NastyDgmSocket(networknastiness);
        c150debug->printf(C150APPLICATION, "Ready to accept messages");

        while (1)
        {
            //
            // Read a packet
            // -1 in size below is to leave room for null
            //
            readlen = sock->read(incomingMessage, sizeof(incomingMessage) - 1);
            if (readlen == 0)
            {
                c150debug->printf(C150APPLICATION, "Does not recive the file, trying again");
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
            c150debug->printf(C150APPLICATION, "Successfully read %d bytes. Message=\"%s\"",
                              readlen, incoming.c_str());

            //
            //  create the message to return       
            //
            if (strspn(incomingMessage, "0123456789") == strlen(incomingMessage))
            {
                // check the argument of incmoing messgae is 1 return the filename
                response = argv[TargetDir];
            }

            else
            { // check the argument of incmoing messgae is 1 return the filename
                // recived the sha of file
                //
                // Loop copying the files
                //
                // copyfile takes name of target file
                //

                TARGET = opendir(argv[TargetDir]);
                if (TARGET == NULL)
                {
                    fprintf(stderr, "Error opening target directory %s \n", argv[TargetDir]);
                    exit(8);
                }
                // split the incomingmessage by delim
                split(incomingMessage, delim, incomingfile);
                while ((sourceFile = readdir(TARGET)) != NULL)
                {
                    // compare string
                    // skip the file not been generated the checksum
                    if ((strcmp(sourceFile->d_name, (char*)incomingfile[0]) != 0))
                        continue;

                    // skip the . and .. names
                    if ((strcmp(sourceFile->d_name, ".") == 0) ||
                        (strcmp(sourceFile->d_name, "..") == 0))
                        continue; // never copy . or ..

                    // generate the sha code for inputfile
                    checksum((char *)sourceFile->d_name, shaComputedHash);
                    //
                    // begin end-to-end check
                    //
                    // to do :
                    // - check one by one
                    // checksum to string
                    
                    generatechecksum = convertToString(shaComputedHash);
                    if (generatechecksum.compare(incomingfile[1])) == 0)
                    {

                        response = "Success";
                    }
                    else
                    {
                        response = "Fail";
                    }
                }
                c150debug->printf(C150APPLICATION, "Responding with message=\"%s\"",
                                  response.c_str());
                sock->write(response.c_str(), response.length() + 1);
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

// ------------------------------------------------------
//
//                   makeFileName
//
// Put together a directory and a file name, making
// sure there's a / in between
//
// ------------------------------------------------------

// string
// makeFileName(string dir, string name)
// {
//     stringstream ss;

//     ss << dir;
//     // make sure dir name ends in /
//     if (dir.substr(dir.length() - 1, 1) != "/")
//         ss << '/';
//     ss << name;      // append file name to dir
//     return ss.str(); // return dir/name
// }

// ------------------------------------------------------
//
//                   checkDirectory
//
//  Make sure directory exists
//
// ------------------------------------------------------

// void checkDirectory(char *dirname)
// {
//     struct stat statbuf;
//     if (lstat(dirname, &statbuf) != 0)
//     {
//         fprintf(stderr, "Error stating supplied source directory %s\n", dirname);
//         exit(8);
//     }

//     if (!S_ISDIR(statbuf.st_mode))
//     {
//         fprintf(stderr, "File %s exists but is not a directory\n", dirname);
//         exit(8);
//     }
// }

// ------------------------------------------------------
//
//                   isFile
//
//  Make sure the supplied file is not a directory or
//  other non-regular file.
//
// ------------------------------------------------------

// bool isFile(string fname)
// {
//     const char *filename = fname.c_str();
//     struct stat statbuf;
//     if (lstat(filename, &statbuf) != 0)
//     {
//         fprintf(stderr, "isFile: Error stating supplied source file %s\n", filename);
//         return false;
//     }

//     if (!S_ISREG(statbuf.st_mode))
//     {
//         fprintf(stderr, "isFile: %s exists but is not a regular file\n", filename);
//         return false;
//     }
//     return true;
// }

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
 
void setUpDebugLogging(const char *logname, int argc, char *argv[]) {

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
//                   string tools
//
// modified the strings
//
// ------------------------------------------------------

string convertToString(unsigned char* a)
{
    string s(reinterpret_cast<char*>(a));
 
    return s;
}