#include "cpu.h"

void readInFilenames(std::vector<std::string>& filenames);

struct CPU::State readInState(const std::string& filename);

void readInNESTestStates(std::vector<struct CPU::State>& states,
    std::vector<uint32_t>& instructions, std::vector<std::string>& testLogs);

void runProgram(CPU& cpu, const std::string& filename);

void runTests(CPU& cpu, std::vector<std::string>& filenames);

void runNESTest(CPU& cpu);

void runNESGame(CPU& cpu);

int main(int argc, char* argv[]) {
    CPU cpu;
    if (argc == 1) {
        cpu.setHaltAtBrk(true);
        std::vector<std::string> filenames;
        readInFilenames(filenames);
        runTests(cpu, filenames);
        runNESTest(cpu);
        cpu.setHaltAtBrk(false);
        runNESGame(cpu);
    } else if (argc == 2) {
        cpu.setMute(false);
        std::string filename(argv[1]);
        runProgram(cpu, filename);
    } else {
        std::cerr << "Unexpected number of arguments\n";
        exit(1);
    }
    return 0;
}

void readInFilenames(std::vector<std::string>& filenames) {
    std::string filenameList = "test/filenames";
    std::string line;
    std::ifstream file(filenameList.c_str());

    if (!file.is_open()) {
        std::cerr << "Error reading in file\n";
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

struct CPU::State readInState(const std::string& filename) {
    struct CPU::State state;
    uint16_t data[5];
    unsigned int dataIndex = 0;
    std::string line;
    std::string stateFilename = filename + ".state";
    std::ifstream file(stateFilename.c_str());

    if (!file.is_open()) {
        std::cerr << "Error reading in file\n";
        exit(1);
    }

    while (file.good()) {
        getline(file, line);
        std::string substring = "";

        for (char currentChar : line) {
            if (currentChar == '/' || currentChar == ' ') {
                break;
            }
            substring += currentChar;
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
        std::cerr << "Unexpected number of state fields\n";
        exit(1);
    }

    state.pc = data[0];
    state.sp = data[1];
    state.a = data[2];
    state.x = data[3];
    state.y = data[4];

    return state;
}

void readInNESTestStates(std::vector<struct CPU::State>& states,
        std::vector<uint32_t>& instructions,
        std::vector<std::string>& testLogs) {
    std::string filename = "nestest.log";
    std::string line;
    std::ifstream file(filename.c_str());

    if (!file.is_open()) {
        std::cerr << "Error reading in file\n";
        exit(1);
    }

    while (file.good()) {
        struct CPU::State state;
        uint32_t inst = 0;
        std::string substring;
        getline(file, line);

        if (line.size() <= 1) {
            break;
        }

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

void runProgram(CPU& cpu, const std::string& filename) {
    cpu.readInInst(filename);

    std::string input;
    std::cin >> input;

    while (!cpu.isEndOfProgram()) {
        if (input == "c" || input == "continue") {
            cpu.setHaltAtBrk(true);
            cpu.print(false);
            cpu.step(nullptr, nullptr);
            cpu.print(true);
            if (!cpu.isEndOfProgram()) {
                std::cout << "\n";
            }
        } else if (input == "s" || input == "step") {
            cpu.print(false);
            cpu.step(nullptr, nullptr);
            cpu.print(true);
            std::cin >> input;
        } else {
            std::cout << "Exiting\n";
            exit(0);
        }
    }

    std::cout << "End of the program\n";
}

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
                (!cpu.isHaltAtBrk() &&
                cpu.getTotalCycles() < state.totalCycles)) {
            cpu.step(nullptr, nullptr);
        }

        if (!cpu.isHaltAtBrk()) {
            cpu.setHaltAtBrk(true);
        }

        if (!cpu.compareState(state)) {
            failedTests.push_back(testNum);
        }
    }

    if (failedTests.size() != filenames.size()) {
        unsigned int numPassedTests = filenames.size() - failedTests.size();
        std::cout << "Passed " << numPassedTests << " tests\n";
    }

    for (unsigned int failedTest : failedTests) {
        std::string& currentFilename = filenames[failedTest];
        unsigned int sizeMinusDir = currentFilename.size() - 5;
        std::cout << "Failed test \"" <<
            currentFilename.substr(5, sizeMinusDir) << "\"\n";
    }
}

void runNESTest(CPU& cpu) {
    std::string filename = "nestest.nes";
    std::vector<struct CPU::State> states;
    std::vector<uint32_t> instructions;
    std::vector<std::string> testLogs;
    unsigned int instNum = 0;
    bool passed = true;

    cpu.clear();

    cpu.readInINES(filename);
    readInNESTestStates(states, instructions, testLogs);

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

    uint8_t validOpResult = cpu.getValidOpResult();
    uint8_t invalidOpResult = cpu.getInvalidOpResult();

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

void runNESGame(CPU& cpu) {
    std::string filename = "donkey-kong.nes";
    cpu.clear();

    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_Window* window = SDL_CreateWindow("SDL2", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, FRAME_WIDTH, FRAME_HEIGHT, SDL_WINDOW_OPENGL);
    if (window == nullptr) {
        std::cerr << "Could not create window\n" << SDL_GetError();
        exit(1);
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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

    cpu.readInINES(filename);
    SDL_Event event;
    bool running = true;
    while (running) {
        unsigned int ppuCycles = cpu.getTotalPPUCycles();
        while (cpu.getTotalPPUCycles() < ppuCycles + 341 * 262 / 300) {
            cpu.step(renderer, texture);
        }

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_KEYDOWN:
                    cpu.writeIO(event);
                    break;
                case SDL_KEYUP:
                    cpu.writeIO(event);
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