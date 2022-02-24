#include <vector>

#include "cpu.h"

void readInFilenames(std::vector<std::string>& filenames);

struct CPUState readInState(std::string& filename);

void runProgram(CPU& cpu, std::string& filename);

void runTests(CPU& cpu, std::vector<std::string>& filenames);

int main(int argc, char* argv[]) {
    CPU cpu;
    if (argc == 1) {
        std::vector<std::string> filenames;
        readInFilenames(filenames);
        runTests(cpu, filenames);
    } else if (argc == 2) {
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
    uint16_t data[7];
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

void runProgram(CPU& cpu, std::string& filename) {
    cpu.readInInst(filename);
    std::string input;
    std::cin >> input;
    while (!cpu.isEndOfProgram()) {
        if (input == "c" || input == "continue") {
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
        if (testNum > 0) {
            cpu.reset(true);
        }

        cpu.readInInst(currentFilename);
        while (!cpu.isEndOfProgram()) {
            cpu.step();
        }

        struct CPUState state = readInState(currentFilename);
        if (!cpu.compareState(state)) {
            failedTests.push_back(testNum);
        }
    }

    if (failedTests.size() == 0) {
        std::cout << "Passed all tests" << std::endl;
    }

    for (int i = 0; i < failedTests.size(); ++i) {
        std::string& currentFilename = filenames[failedTests[i]];
        unsigned int sizeMinusDir = currentFilename.size() - 5;
        std::cout << "Failed test \"" <<
            currentFilename.substr(5, sizeMinusDir) << "\"" << std::endl;
    }
}