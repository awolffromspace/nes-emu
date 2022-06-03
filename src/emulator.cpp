#include "cpu.h"

#define WINDOW_SIZE_MULTIPLIER 1

void readInFilenames(std::vector<std::string>& filenames);

struct CPU::State readInState(const std::string& filename);

void readInNESTestStates(std::vector<struct CPU::State>& states,
    std::vector<uint32_t>& instructions, std::vector<std::string>& testLogs);

void runProgram(CPU& cpu, const std::string& filename);

void stepAndPrint(CPU& cpu, bool showPPU);

void runTests(CPU& cpu, std::vector<std::string>& filenames);

void runNESTest(CPU& cpu);

void runNESGame(CPU& cpu, const std::string& filename);

int main(int argc, char* argv[]) {
    CPU cpu;
    if (argc == 1) {
        cpu.setHaltAtBrk(true);
        std::vector<std::string> filenames;
        readInFilenames(filenames);
        runTests(cpu, filenames);
        cpu.clear();
        runNESTest(cpu);
    } else if (argc == 2) {
        std::string filename(argv[1]);
        if (filename.size() < 5) {
            std::cerr << "Game filename is too short or missing its filename extension\n";
            exit(1);
        }
        std::string fileFormat = filename.substr(filename.size() - 4, 4);
        if (fileFormat != ".nes") {
            std::cerr << "Wrong filename extension\n";
            exit(1);
        }
        runNESGame(cpu, filename);
    } else if (argc == 3) {
        std::string debugStr = "debug";
        std::string arg(argv[2]);
        if (arg != debugStr) {
            std::cerr << "Unexpected argument\n";
            exit(1);
        }
        cpu.setMute(false);
        std::string filename(argv[1]);
        runProgram(cpu, filename);
    } else {
        std::cerr << "Unexpected number of arguments\n";
        exit(1);
    }
    return 0;
}

// Adds each filename in test/filenames to a vector

void readInFilenames(std::vector<std::string>& filenames) {
    std::string filenameList = "test/filenames";
    std::ifstream file(filenameList.c_str());
    if (!file.is_open()) {
        std::cerr << "Error reading in file\n";
        exit(1);
    }

    std::string line;
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

// For each unit test in the test directory, this function converts the test's .state file into a
// State struct for comparing CPU states. Initially, the data is stored in an array so that the
// while loop logic can be better generalized

struct CPU::State readInState(const std::string& filename) {
    std::string stateFilename = filename + ".state";
    std::ifstream file(stateFilename.c_str());
    if (!file.is_open()) {
        std::cerr << "Error reading in file\n";
        exit(1);
    }

    struct CPU::State state;
    uint16_t data[5];
    unsigned int dataIndex = 0;
    std::string line;
    while (file.good()) {
        getline(file, line);
        std::string substring = "";
        for (char currentChar : line) {
            if (currentChar == '/' || currentChar == ' ') {
                break;
            }
            substring += currentChar;
        }
        if (dataIndex < 5) {
            data[dataIndex] = std::stoul(substring, nullptr, 16);
        } else if (dataIndex == 5) {
            state.p = std::stoul(substring, nullptr, 2);
        } else if (dataIndex == 6) {
            state.totalCycles = std::stoul(substring, nullptr, 10);
        } else {
            std::cerr << "Unexpected number of state fields\n";
            exit(1);
        }
        ++dataIndex;
        substring = "";
    }
    file.close();

    state.pc = data[0];
    state.sp = data[1];
    state.a = data[2];
    state.x = data[3];
    state.y = data[4];
    return state;
}

// Converts each line in nestest.log to a State struct for comparing CPU states. The current
// instruction for each line and the original line from nestest.log are also added to vectors for
// easier debugging

void readInNESTestStates(std::vector<struct CPU::State>& states,
        std::vector<uint32_t>& instructions, std::vector<std::string>& testLogs) {
    std::string filename = "nestest.log";
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        std::cerr << "Error reading in file\n";
        exit(1);
    }

    std::string line;
    while (file.good()) {
        getline(file, line);
        if (line.size() <= 1) {
            break;
        }

        struct CPU::State state;
        uint32_t inst = 0;
        std::string substring;

        substring = line.substr(0, 4);
        state.pc = std::stoul(substring, nullptr, 16) + 1;

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

// Runs the .NES or instruction file without graphics for debugging. "continue" input runs the
// program until the BRK instruction is executed. "step" input runs the program for one CPU cycle

void runProgram(CPU& cpu, const std::string& filename) {
    bool nesFile = false;
    if (filename.size() > 4) {
        std::string fileFormat = filename.substr(filename.size() - 4, 4);
        if (fileFormat == ".nes") {
            cpu.readInINES(filename);
            nesFile = true;
        }
    }
    if (!nesFile) {
        cpu.readInInst(filename);
    }

    std::string commands = "Enter 's' or 'step', 'c' or 'continue', 'b' or 'break', 'p' or 'ppu', "
        "or 'q' or 'quit': ";
    std::string input;
    bool showPPU = false;
    std::cout << commands;
    std::cin >> input;
    while (!cpu.isEndOfProgram()) {
        if (input == "c" || input == "continue") {
            cpu.setHaltAtBrk(true);
            stepAndPrint(cpu, showPPU);
            if (!cpu.isEndOfProgram()) {
                std::cout << "\n";
            }
        } else if (input == "s" || input == "step") {
            stepAndPrint(cpu, showPPU);
        } else if (input == "b" || input == "break") {
            std::cout << "PC to break at: ";
            std::cin >> input;
            uint16_t targetPC = std::stoul(input, nullptr, 16);
            do {
                stepAndPrint(cpu, showPPU);
            } while (cpu.getPC() != targetPC);
        } else if (input == "p" || input == "ppu") {
            showPPU = !showPPU;
            if (showPPU) {
                std::cout << "PPU state will be printed\n";
            } else {
                std::cout << "PPU state will not be printed\n";
            }
        } else {
            std::cout << "Exiting\n";
            exit(0);
        }

        if (input != "c" && input != "continue") {
            std::cout << commands;
            std::cin >> input;
        }
    }
    std::cout << "End of the program\n";
}

void stepAndPrint(CPU& cpu, bool showPPU) {
    cpu.print(false);
    cpu.step(nullptr, nullptr);
    cpu.print(true);
    if (showPPU) {
        cpu.printPPU();
    }
}

// Given the unit test filenames, this function runs all tests and compares the CPU's state with the
// test's .state file. Any failed tests are printed out

void runTests(CPU& cpu, std::vector<std::string>& filenames) {
    std::vector<unsigned int> failedTests;
    for (unsigned int testNum = 0; testNum < filenames.size(); ++testNum) {
        std::string& currentFilename = filenames[testNum];
        struct CPU::State state = readInState(currentFilename);
        if (testNum > 0) {
            cpu.clear();
        }
        cpu.readInInst(currentFilename);
        if (currentFilename.size() >= 8) {
            std::string testType = currentFilename.substr(5, 3);
            if (testType == "brk") {
                cpu.setHaltAtBrk(false);
            }
        }
        while ((cpu.isHaltAtBrk() && !cpu.isEndOfProgram()) ||
                (!cpu.isHaltAtBrk() && cpu.getTotalCycles() < state.totalCycles)) {
            cpu.step(nullptr, nullptr);
        }
        if (!cpu.compareState(state)) {
            failedTests.push_back(testNum);
        }
        if (!cpu.isHaltAtBrk()) {
            cpu.setHaltAtBrk(true);
        }
    }

    if (failedTests.size() != filenames.size()) {
        unsigned int numPassedTests = filenames.size() - failedTests.size();
        std::cout << "Passed " << numPassedTests << " tests\n";
    }

    for (unsigned int failedTest : failedTests) {
        std::string& currentFilename = filenames[failedTest];
        unsigned int sizeMinusDir = currentFilename.size() - 5;
        std::cout << "Failed test \"" << currentFilename.substr(5, sizeMinusDir) << "\"\n";
    }
}

// Runs the nestest.nes system test and compares the CPU's state with the state of each line in
// nestest.log. If a state doesn't match, it gets printed out

void runNESTest(CPU& cpu) {
    std::string filename = "nestest.nes";
    std::vector<struct CPU::State> states;
    std::vector<uint32_t> instructions;
    std::vector<std::string> testLogs;
    cpu.readInINES(filename);
    readInNESTestStates(states, instructions, testLogs);

    unsigned int instNum = 0;
    bool passed = true;
    while (!cpu.isEndOfProgram() && instNum < states.size()) {
        if (cpu.getOpCycles() == 1) {
            struct CPU::State state = states[instNum];
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
        cpu.step(nullptr, nullptr);
    }

    // nestest.nes puts a nonzero value in the RAM address $0002 if any valid opcodes fail and in
    // $0003 if any invalid/unofficial opcodes fail, so they get printed out as well if they're ever
    // nonzero
    uint8_t validOpResult = cpu.getValidOpResult();
    uint8_t invalidOpResult = cpu.getInvalidOpResult();
    if (validOpResult != 0) {
        std::cout << "0x2: " << std::hex << (unsigned int) validOpResult << "\n";
        passed = false;
    }
    if (invalidOpResult != 0) {
        std::cout << "0x3: " << (unsigned int) invalidOpResult << "\n" << std::dec;
        passed = false;
    }
    if (passed) {
        std::cout << "Passed nestest.nes\n";
    }
}

// Runs the .NES file with graphics and I/O

void runNESGame(CPU& cpu, const std::string& filename) {
    cpu.readInINES(filename);

    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_Window* window = SDL_CreateWindow("SDL2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        FRAME_WIDTH * WINDOW_SIZE_MULTIPLIER, FRAME_HEIGHT * WINDOW_SIZE_MULTIPLIER,
        SDL_WINDOW_OPENGL);
    if (window == nullptr) {
        std::cerr << "Could not create window\n" << SDL_GetError();
        exit(1);
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED |
        SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        std::cerr << "Could not create renderer\n" << SDL_GetError();
        exit(1);
    }
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, FRAME_WIDTH, FRAME_HEIGHT);
    if (texture == nullptr) {
        std::cerr << "Could not create texture\n" << SDL_GetError();
        exit(1);
    }

    SDL_Event event;
    bool running = true;
    while (running) {
        unsigned int ppuCycles = cpu.getTotalPPUCycles();
        // Ensure that the total PPU cycles will always be less than ppuCycles +
        // PPU_CYCLES_PER_FRAME
        if (ppuCycles > UINT_MAX - PPU_CYCLES_PER_FRAME) {
            cpu.clearTotalPPUCycles();
            ppuCycles = cpu.getTotalPPUCycles();
        }
        // Run CPU (and other components) for however many cycles it takes to render one frame
        // without polling for I/O. I/O is polled only every frame rather than anything more
        // frequent (e.g., every CPU cycle) to reduce the lag from calling SDL_PollEvent too much
        while (cpu.getTotalPPUCycles() < ppuCycles + PPU_CYCLES_PER_FRAME) {
            cpu.step(renderer, texture);
        }

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_KEYDOWN:
                    cpu.updateButton(event);
                    break;
                case SDL_KEYUP:
                    cpu.updateButton(event);
                    break;
                case SDL_QUIT:
                    running = false;
            }
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}