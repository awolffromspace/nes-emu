#include <vector>

#include "cpu.h"

void readInFilenames(std::vector<std::string>& filenames);

struct CPUState readInState(std::string& filename);

void readInNESTestStates(std::vector<struct CPUState>& states,
    std::vector<uint32_t>& instructions, std::vector<std::string>& testLogs);

void runProgram(CPU& cpu, std::string& filename);

void runTests(CPU& cpu, std::vector<std::string>& filenames);

void runNESTest(CPU& cpu);

int main(int argc, char* argv[]) {
    CPU cpu;
    if (argc == 1) {
        cpu.setHaltAtBrk(true);

        std::vector<std::string> filenames;
        readInFilenames(filenames);
        runTests(cpu, filenames);
        runNESTest(cpu);
    } else if (argc == 2) {
        cpu.setMute(false);

        std::string filename(argv[1]);
        runProgram(cpu, filename);
    } else {
        std::cerr << "Unexpected number of arguments" << std::endl;
        exit(1);
    }
    return 0;
}

void readInFilenames(std::vector<std::string>& filenames) {
    std::string filenameList = "test/filenames";
    std::string line;
    std::ifstream file(filenameList.c_str());

    if (!file.is_open()) {
        std::cerr << "Error reading in file" << std::endl;
        exit(1);
    }

    while (file.good()) {
        getline(file, line);
        if (line.at(line.size() - 1) == '\n') {
            filenames.push_back("test/" + line.substr(0, line.size() - 1));
        } else {
            filenames.push_back("test/" + line);
        }
    }

    file.close();
}

struct CPUState readInState(std::string& filename) {
    struct CPUState state;
    uint16_t data[5];
    int dataIndex = 0;
    std::string line;
    std::string stateFilename = filename + ".state";
    std::ifstream file(stateFilename.c_str());

    if (!file.is_open()) {
        std::cerr << "Error reading in file" << std::endl;
        exit(1);
    }

    while (file.good()) {
        getline(file, line);
        std::string substring = "";

        for (int i = 0; i < line.size(); ++i) {
            if (line.at(i) == '/' || line.at(i) == ' ') {
                break;
            }
            substring += line.at(i);
        }

        if (dataIndex == 5) {
            state.p = std::stoul(substring, nullptr, 2);
        } else if (dataIndex == 6) {
            state.totalCycles = std::stoul(substring, nullptr, 10);
        } else {
            data[dataIndex] = std::stoul(substring, nullptr, 16);
        }

        ++dataIndex;
        substring = "";
    }

    file.close();

    if (dataIndex < 7) {
        std::cerr << "Unexpected number of state fields" << std::endl;
        exit(1);
    }

    state.pc = data[0];
    state.sp = data[1];
    state.a = data[2];
    state.x = data[3];
    state.y = data[4];

    return state;
}

void readInNESTestStates(std::vector<struct CPUState>& states,
        std::vector<uint32_t>& instructions,
        std::vector<std::string>& testLogs) {
    std::string filename = "nestest.log";
    std::string line;
    std::ifstream file(filename.c_str());

    if (!file.is_open()) {
        std::cerr << "Error reading in file" << std::endl;
        exit(1);
    }

    while (file.good()) {
        struct CPUState state;
        uint32_t inst = 0;
        std::string substring;
        getline(file, line);

        if (line.size() <= 1) {
            break;
        }

        substring = line.substr(0, 4);
        state.pc = std::stoul(substring, nullptr, 16) + 0x1;

        substring = line.substr(6, 2);
        inst = std::stoul(substring, nullptr, 16);

        substring = line.substr(9, 2);
        if (substring.at(0) != ' ') {
            inst = inst << 8;
            inst |= std::stoul(substring, nullptr, 16);
        }

        substring = line.substr(12, 2);
        if (substring.at(0) != ' ') {
            inst = inst << 8;
            inst |= std::stoul(substring, nullptr, 16);
        }
        
        substring = line.substr(50, 2);
        state.a = std::stoul(substring, nullptr, 16);

        substring = line.substr(55, 2);
        state.x = std::stoul(substring, nullptr, 16);

        substring = line.substr(60, 2);
        state.y = std::stoul(substring, nullptr, 16);

        substring = line.substr(65, 2);
        state.p = std::stoul(substring, nullptr, 16);

        substring = line.substr(71, 2);
        state.sp = std::stoul(substring, nullptr, 16);

        substring = line.substr(90, line.size() - 90);
        state.totalCycles = std::stoul(substring, nullptr, 10) - 6;

        states.push_back(state);
        instructions.push_back(inst);
        testLogs.push_back(line);
    }

    file.close();
}

void runProgram(CPU& cpu, std::string& filename) {
    cpu.readInInst(filename);

    std::string input;
    std::cin >> input;

    while (!cpu.isEndOfProgram()) {
        if (input == "c" || input == "continue") {
            cpu.setHaltAtBrk(true);
            cpu.print(false);
            cpu.step();
            cpu.print(true);
            if (!cpu.isEndOfProgram()) {
                std::cout << "\n";
            }
        } else if (input == "s" || input == "step") {
            cpu.print(false);
            cpu.step();
            cpu.print(true);
            std::cin >> input;
        } else {
            std::cout << "Exiting" << std::endl;
            exit(0);
        }
    }

    std::cout << "End of the program" << std::endl;
}

void runTests(CPU& cpu, std::vector<std::string>& filenames) {
    std::vector<unsigned int> failedTests;
    for (int testNum = 0; testNum < filenames.size(); ++testNum) {
        std::string& currentFilename = filenames[testNum];
        struct CPUState state = readInState(currentFilename);

        if (testNum > 0) {
            cpu.reset();
        }

        cpu.readInInst(currentFilename);

        if (currentFilename.size() >= 8) {
            std::string testType = currentFilename.substr(5, 3);
            if (testType == "brk") {
                cpu.setHaltAtBrk(false);
            }
        }

        while ((cpu.isHaltAtBrk() && !cpu.isEndOfProgram()) ||
                (!cpu.isHaltAtBrk() &&
                cpu.getTotalCycles() < state.totalCycles)) {
            cpu.step();
        }

        if (!cpu.isHaltAtBrk()) {
            cpu.setHaltAtBrk(true);
        }

        if (!cpu.compareState(state)) {
            failedTests.push_back(testNum);
        }
    }

    if (failedTests.size() != filenames.size()) {
        int numPassedTests = filenames.size() - failedTests.size();
        std::cout << "Passed " << numPassedTests << " tests" << std::endl;
    }

    for (int i = 0; i < failedTests.size(); ++i) {
        std::string& currentFilename = filenames[failedTests[i]];
        unsigned int sizeMinusDir = currentFilename.size() - 5;
        std::cout << "Failed test \"" <<
            currentFilename.substr(5, sizeMinusDir) << "\"" << std::endl;
    }
}

void runNESTest(CPU& cpu) {
    std::string filename = "nestest.nes";
    std::vector<struct CPUState> states;
    std::vector<uint32_t> instructions;
    std::vector<std::string> testLogs;
    unsigned int instNum = 0;
    bool passed = true;

    cpu.reset();

    cpu.readInINES(filename);
    readInNESTestStates(states, instructions, testLogs);

    while (!cpu.isEndOfProgram() && instNum < states.size()) {
        if (cpu.getOpCycles() == 1) {
            struct CPUState state = states[instNum];
            uint32_t testInst = instructions[instNum];
            uint32_t inst = cpu.getFutureInst();
            std::string testLog = testLogs[instNum];

            if (!cpu.compareState(state) || inst != testInst) {
                std::cout << "Test log: " << testLog << "\nEmulator: ";
                cpu.printStateInst(inst);
                std::cout << "\n";
                passed = false;
            }
            ++instNum;
        }

        cpu.step();
    }

    uint8_t validOpResult = cpu.readMemory(0x2);
    uint8_t invalidOpResult = cpu.readMemory(0x3);

    if (validOpResult != 0) {
        std::cout << "0x2: " << std::hex << (unsigned int) validOpResult <<
            "\n";
        passed = false;
    }

    if (invalidOpResult != 0) {
        std::cout << "0x3: " << (unsigned int) invalidOpResult << "\n" <<
            std::dec;
        passed = false;
    }

    if (passed) {
        std::cout << "Passed nestest.nes\n";
    }
}