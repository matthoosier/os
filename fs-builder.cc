#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct InputFile {

    InputFile (const std::string & name) {
        this->name = name;
        bzero(&this->stat, sizeof(this->stat));
    }

    std::string name;
    struct stat stat;
};

int main (int argc, char *argv[]) {

    std::auto_ptr<std::string>  ptrOutputFileName;
    std::auto_ptr<std::string>  ptrIdentifierName;
    std::vector<InputFile *>    inputFiles;

    /* First handle explicit argument switches */
    for (int whichOption; (whichOption = getopt(argc, argv, "+o:n:")) != -1;) {

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

            case 'n':
                if (ptrIdentifierName.get()) {
                    std::cout << "Output C identifier name already set (" << *ptrIdentifierName << ")" << std::endl;
                    ::exit(1);
                }
                else {
                    ptrIdentifierName.reset(new std::string(optarg));
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
        inputFiles.push_back(new InputFile(argv[index]));
    }

    if (!ptrOutputFileName.get()) {
        std::cout << "Output file (-o <filename>) required" << std::endl;
        ::exit(1);
    }

    if (!ptrIdentifierName.get()) {
        std::cout << "C identifier name (-n <C variable name>) required" << std::endl;
        ::exit(1);
    }

    std::ofstream outputFile;
    outputFile.open(ptrOutputFileName->c_str(), std::ios::out);

    outputFile << "#include <muos/compiler.h>" << std::endl << std::endl;
    outputFile << "#include <kernel/image.h>" << std::endl;
    outputFile << "#include <kernel/array.h>" << std::endl << std::endl;

    outputFile << "#ifdef __cplusplus" << std::endl;
    outputFile << "\t#define C_EXTERN extern \"C\"" << std::endl;
    outputFile << "#else" << std::endl;
    outputFile << "\t#define C_EXTERN /* nothing */" << std::endl;
    outputFile << "#endif /* __cplusplus */" << std::endl;
    outputFile << std::endl;
    
    /* Get size information for each file */
    for (unsigned int i = 0; i < inputFiles.size(); i++) {

        struct stat buf;

        if (stat(inputFiles[i]->name.c_str(), &buf) != -1) {
            inputFiles[i]->stat = buf;
        }
        else {
            std::cout << "Can't stat " << inputFiles[i]->name << "(" << strerror(errno) << ")" << std::endl;
        }
    }

    /* Write out individual file payloads */
    for (unsigned int i = 0; i < inputFiles.size(); i++) {
        std::ifstream infile;
        infile.open(inputFiles[i]->name.c_str(), std::ios::in | std::ios::binary);

        outputFile << "static const uint8_t entry_" << i << "[] = {" << std::endl;

        for (size_t j = 0; j < inputFiles[i]->stat.st_size && !infile.eof(); j++) {
            char memblock;
            infile.read(&memblock, 1);
            outputFile << "\t" << static_cast<int>(memblock) << ", " << std::endl;
        }

        outputFile
                << "}; COMPILER_ASSERT(N_ELEMENTS(entry_"
                << i
                << ") == "
                << inputFiles[i]->stat.st_size
                << ");"
                << std::endl << std::endl;
    }

    /* Write out the individual entries */
    outputFile << "static const struct ImageEntry entries[] = {" << std::endl;

    for (unsigned int i = 0; i < inputFiles.size(); i++) {
        outputFile << "\t{ "
                   << "&entry_" << i << "[0], "
                   << inputFiles[i]->stat.st_size
                   << ", "
                   << "\"" << inputFiles[i]->name << "\""
                   << " }, "
                   << std::endl;
    }

    outputFile << "};" << std::endl << std::endl;
    outputFile << "COMPILER_ASSERT(N_ELEMENTS(entries) == " << inputFiles.size() << ");"
               << std::endl
               << std::endl;

    /* Write out the only publicly visible symbol in the file */
    outputFile << "C_EXTERN struct Image " << *ptrIdentifierName << " = {" << std::endl;
    outputFile << "\t" << inputFiles.size() << "," << std::endl;
    outputFile << "\t&entries[0]" << std::endl;
    outputFile << "};" << std::endl;

    outputFile.close();
}
