#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

#if defined(_WIN32)
    /* This at least works on MinGW */
    #include <getopt.h>
#endif

struct InputFile {

    InputFile (const std::string & name)
    {
        this->mFullName = name;
        this->mBaseName = name;

        std::ifstream file;
        file.open(name.c_str(), std::ios::binary | std::ios::in);

        if (file.good()) {
            mExists = true;
            file.seekg(0, std::ios::end);
            mSize = file.tellg();
            file.close();
        } else {
            mExists = false;
        }
    }

    std::string mBaseName;
    std::string mFullName;
    size_t mSize;
    bool mExists;
};

int main (int argc, char *argv[]) {

    std::auto_ptr<std::string>  ptrOutputFileName;
    std::vector<InputFile *>    inputFiles;

    /* First handle explicit argument switches */
    for (int whichOption; (whichOption = getopt(argc, argv, "+o:")) != -1;) {

        switch (whichOption) {

            case 'o':
                if (ptrOutputFileName.get()) {
                    std::cout << "Output filename already set (" << *ptrOutputFileName << ")" << std::endl;
                    ::exit(1);
                }
                else {
                    ptrOutputFileName.reset(new std::string(optarg));
                }
                break;

            default:
                /* getopt() has already printed an error message */
                ::exit(1);
                break;
        }
    }

    /* Collect all unprocessed arguments as a list of files to be packed */
    for (unsigned int index = optind; index < argc; index++) {

        InputFile * input = new InputFile(argv[index]);

        if (input->mExists) {
            inputFiles.push_back(input);
        } else {
            std::cerr << input->mFullName << " does not exist\n";
            ::exit(1);
        }
    }

    if (!ptrOutputFileName.get()) {
        std::cout << "Output file (-o <filename>) required" << std::endl;
        ::exit(1);
    }

    std::ofstream outputFile;
    outputFile.open(ptrOutputFileName->c_str(),
                    std::ios::out | std::ios::trunc | std::ios::binary);

    /* Write out each file in sequence */
    for (unsigned int i = 0; i < inputFiles.size(); i++) {
        std::ifstream infile;
        infile.open(inputFiles[i]->mFullName.c_str(), std::ios::in | std::ios::binary);

        /* Length of file name. Save as big-endian uint32_t */
        uint32_t name_len = inputFiles[i]->mBaseName.length();
        uint8_t name_len_be[4] = {((name_len & 0xff000000) >> 24),
                                  ((name_len & 0x00ff0000) >> 16),
                                  ((name_len & 0x0000ff00) >> 8),
                                  ((name_len & 0x000000ff) >> 0)};
        outputFile.write((char const *)name_len_be, sizeof(name_len_be));

        /* File name content */
        outputFile.write(inputFiles[i]->mBaseName.c_str(),
                         inputFiles[i]->mBaseName.length());

        /* Payload length. Also is big-endian uint32_t */
        uint32_t payload_len = inputFiles[i]->mSize;
        uint8_t payload_len_be[4] = {((payload_len & 0xff000000) >> 24),
                                     ((payload_len & 0x00ff0000) >> 16),
                                     ((payload_len & 0x0000ff00) >> 8),
                                     ((payload_len & 0x000000ff) >> 0)};
        outputFile.write((char const *)payload_len_be, sizeof(payload_len_be));

        /* Payload */
        for (size_t remaining = inputFiles[i]->mSize;
             remaining > 0;)
        {
            char buf[4096];
            size_t transfer_size = std::min(sizeof(buf), remaining);

            infile.read(&buf[0], transfer_size);
            outputFile.write(&buf[0], transfer_size);

            remaining -= transfer_size;
        }

     }

    outputFile.close();

    for (std::vector<InputFile *>::iterator i = inputFiles.begin();
         i != inputFiles.end(); ++i)
    {
        InputFile * f = *i;
        delete f;
    }
}
