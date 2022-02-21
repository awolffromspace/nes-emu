#include "cpu.h"

int main(int argc, char* argv[]) {
    CPU cpu;
    std::string filenames[] = {"test/adc-imm"};
    int totalPrograms = 1;
    bool testMode = false;
    if (argc == 1) {
        testMode = true;
        totalPrograms = 1; // Size of filenames
    } else if (argc == 2) {
        std::string filename(argv[1]);
        cpu.readInInst(filename);
    } else {
        std::cout << "Unexpected number of arguments" << std::endl;
        exit(1);
    }

    std::string input;
    if (!testMode) {
        std::cin >> input;
    }
    for (int currentProgram = 0; currentProgram < totalPrograms;
            ++currentProgram) {
        if (currentProgram > 0) {
            cpu.reset();
        }

        if (testMode) {
            cpu.readInInst(filenames[currentProgram]);
        }

        while (!cpu.isEndOfProgram()) {
            if (testMode || input == "c" || input == "continue") {
                cpu.step();
                if (!cpu.isEndOfProgram()) {
                    std::cout << "\n";
                }
            } else if (input == "s" || input == "step") {
                cpu.step();
                std::cin >> input;
            } else {
                std::cout << "Exiting" << std::endl;
                exit(0);
            }
        }

        std::cout << "End of the program" << std::endl;
    }

    return 0;
}