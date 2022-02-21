//        COMMAND LINE
//
//              fileclient <server> <networknastiness> <filenastiness> <srcdir>

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

//
// Always use namespace C150NETWORK with COMP 150 IDS framework!
//
using namespace std;         // for C++ std library
using namespace C150NETWORK; // for all the comp150 utilities

const int serverArg = 1; // server name is 1st arg
const int sourceDir = 4; // Source directory is the 4th arg

void copyFile(string sourceDir, string fileName, string targetDir, int nastiness); // fwd decl
bool isFile(string fname);
void checkDirectory(char *dirname);
void checksum(char filenames[], char shaComputedHash[]);

int main(int argc, char *argv[])
{
    int networknastiness;
    int filenastiness;
    DIR *SRC;                  // Unix descriptor for open directory
    DIR *TARGET;               // Unix descriptor for target
    struct dirent *sourceFile; // Directory entry for source file

    ssize_t readlen;           // amount of data read from socket
    char incomingFileDic[512]; // received message data

    stirng requestCopy;                // request the server to copy file
    unsigned char shaComputedHash[20]; // hash goes here
    char *bufferMessage;               // store the filename and checksum
    size_t bufferLen;
    //
    //  Set up debug message logging
    //
    setUpDebugLogging("lientdebug.txt", argc, argv);

    // grade assignment
    GRADEME(argc, argv);

    try
    {

        //
        // Check command line and parse arguments
        //
        if (argc != 4)
        {
            fprintf(stderr, "Correct syntxt is: %s <server> <networknastiness> <filenastiness> <srcdir>\n", argv[0]);
            exit(1);
        }

        if (strspn(argv[2], "0123456789") != strlen(argv[1]))
        {
            fprintf(stderr, "NetworkNastiness %s is not numeric\n", argv[1]);
            fprintf(stderr, "Correct syntxt is: %s <server> <networknastiness> <filenastiness> <srcdir>\n", argv[0]);
            exit(4);
        }

        if (strspn(argv[3], "0123456789") != strlen(argv[2]))
        {
            fprintf(stderr, "FileNastiness %s is not numeric\n", argv[1]);
            fprintf(stderr, "Correct syntxt is: %s <server> <networknastiness> <filenastiness> <srcdir>\n", argv[0]);
            exit(4);
        }

        filenastiness = atoi(argv[3]); // convert command line string to integer
        networknastiness = atoi(argv[2]);

        // recieve the target directory
        // Create the socket
        c150debug->printf(C150APPLICATION, "Creating C150DgmSocket");
        C150DgmSocket *sock = new C150DgmSocket();

        // Tell the DGMSocket which server to talk to
        sock->setServerName(argv[serverArg]);

        // Send the message to the server
        c150debug->printf(C150APPLICATION, "%s: Writing message: \"%s\"",
                          argv[0], requestCopy);
        sock->write(requestCopy, strlen(requestCopy) + 1); // +1 includes the null

        // Read the response from the server
        c150debug->printf(C150APPLICATION, "%s: Returned from write, doing read()",
                          argv[0]);
        readlen = sock->read(incomingFileDic, sizeof(incomingFileDic));

        //
        // Make sure source and target dirs exist
        //
        checkDirectory(argv[sourceDir]);
        checkDirectory(incomingFileDic);

        //
        // Open the source directory
        //
        SRC = opendir(argv[sourceDir]);
        if (SRC == NULL)
        {
            fprintf(stderr, "Error opening source directory %s\n", argv[sourceDir]);
            exit(8);
        }

        //
        // Open the target directory (we don't really need to do
        // this here, but it will give cleaner error messages than
        // waiting for individual file writes to fail.
        //
        TARGET = opendir(incomingFileDic);
        if (TARGET == NULL)
        {
            fprintf(stderr, "Error opening target directory %s \n", incomingFileDic);
            exit(8);
        }
        closedir(TARGET); // we just wanted to be sure it was there
                          // SRC dir remains open for loop below

        //
        //  Loop copying the files
        //
        //    copyfile takes name of target file
        //
        while ((sourceFile = readdir(SRC)) != NULL)
        {
            // skip the . and .. names
            if ((strcmp(sourceFile->d_name, ".") == 0) ||
                (strcmp(sourceFile->d_name, "..") == 0))
                continue; // never copy . or ..

            // do the copy -- this will check for and
            // skip subdirectories

            copyFile(argv[sourceDir], sourceFile->d_name, incomingFileDic, nastiness);

            // generate the sha code for inputfile
            checksum((const unsigned char *)sourceFile->d_name, shaComputedHash);
            // merge the filename and checksum
            bufferLen = sourceFile->d_namlen + 21; // the lenth of checksum and slash
            bufferMessage = (char *)malloc(bufferLen);
            sourceFile.fread(bufferMessage, 1, sourceFile->d_namlen);
            bufferMessage = bufferMessage + "#";
            shaComputedHash.fread(bufferMessage, 1, 20);
            // send the filename and checksum
            sock->write(bufferMessage, strlen(bufferMessage) + 1);
            // // send the checksum
            // sock->write(ushaComputedHashns, strlen(shaComputedHash) + 1);

            // free the message file
            free(bufferMessage);
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
    size_t len; // length of the file data
    string errorString;
    char *buffer; // buffer with your file data
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
void checksum(char filename[], char shaComputedHash[])
{
    int i, j;
    ifstream *t;
    stringstream *buffer;

    unsigned char obuf[20];
    printf("SHA1 (\"%s\") = ", filename);
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
