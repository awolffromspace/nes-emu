#include "cpu.h"

#define WINDOW_SIZE_MULTIPLIER 4

void readInFilenames(std::vector<std::string>& filenames);

struct CPU::State readInState(const std::string& filename);

void readInNESTestStates(std::vector<struct CPU::State>& states,
    std::vector<uint32_t>& instructions, std::vector<std::string>& testLogs);

void runProgram(CPU& cpu, const std::string& filename);

void stepAndPrint(CPU& cpu, bool showPPU);

void runInstTests(CPU& cpu, std::vector<std::string>& filenames);

void runNESTests(CPU& cpu);

void runCPUTests(CPU& cpu);

void runPPUTests(CPU& cpu);

void runIndividualTest(CPU& cpu, const std::string& testName, const std::string& testDirectory,
    uint16_t stopPC, uint8_t passedTestResult, uint16_t testResultAddr);

void runNESGame(CPU& cpu, const std::string& filename);

int main(int argc, char* argv[]) {
    CPU cpu;
    if (argc == 1) {
        // The end of the instruction tests are when the program reaches zeroed out memory, and the
        // BRK instruction is 0
        cpu.setHaltAtBrk(true);
        std::vector<std::string> filenames;
        readInFilenames(filenames);
        runInstTests(cpu, filenames);
        cpu.setHaltAtBrk(false);
        cpu.clear();
        runNESTests(cpu);
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
    std::string filenameList = "test/inst/filenames";
    std::ifstream file(filenameList.c_str());
    if (!file.is_open()) {
        std::cerr << "Error reading in file\n";
        exit(1);
    }

    std::string line;
    while (file.good()) {
        getline(file, line);
        if (line[line.size() - 1] == '\n') {
            filenames.push_back("test/inst/" + line.substr(0, line.size() - 1));
        } else {
            filenames.push_back("test/inst/" + line);
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
            // Ignore comments and spaces
            if (currentChar == '/' || currentChar == ' ' || currentChar == '\n') {
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
    std::string filename = "test/nestest/nestest.log";
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
        if (substring[0] != ' ') {
            inst = inst << 8;
            inst |= std::stoul(substring, nullptr, 16);
        }

        substring = line.substr(12, 2);
        if (substring[0] != ' ') {
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

// Runs the .NES or instruction file without graphics for debugging. "step" input runs the program
// for one CPU cycle. "continue" input runs the program until the BRK instruction is executed.
// "break" input runs the program until the CPU reaches the specified PC. "ppu" input displays the
// PPU's state

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

// Runs the CPU for one cycle and prints the debug info

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

void runInstTests(CPU& cpu, std::vector<std::string>& filenames) {
    std::vector<unsigned int> failedTests;
    for (unsigned int testNum = 0; testNum < filenames.size(); ++testNum) {
        std::string& currentFilename = filenames[testNum];
        struct CPU::State state = readInState(currentFilename);
        if (testNum > 0) {
            cpu.clear();
        }
        cpu.readInInst(currentFilename);
        if (currentFilename.size() >= 13) {
            std::string testType = currentFilename.substr(10, 3);
            // If the instruction test uses BRK, then the CPU can't halt the moment it reaches BRK
            if (testType == "brk") {
                cpu.setHaltAtBrk(false);
            }
        }
        // The second group of conditions after the OR is for BRK instruction tests. The CPU will
        // halt after reaching the same number of total cycles as the state file
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
        std::cout << "Passed " << numPassedTests << " instruction tests\n";
    }

    for (unsigned int failedTest : failedTests) {
        std::string& currentFilename = filenames[failedTest];
        unsigned int sizeMinusDir = currentFilename.size() - 5;
        std::cout << "Failed test \"" << currentFilename.substr(5, sizeMinusDir) << "\"\n";
    }
}

void runNESTests(CPU& cpu) {
    runCPUTests(cpu);
    runPPUTests(cpu);
}

// Runs tests that are focused on the CPU

void runCPUTests(CPU& cpu) {
    std::string filename = "test/nestest/nestest.nes";
    std::vector<struct CPU::State> states;
    std::vector<uint32_t> instructions;
    std::vector<std::string> testLogs;
    cpu.readInINES(filename);
    readInNESTestStates(states, instructions, testLogs);

    unsigned int instNum = 0;
    bool passed = true;
    while (!cpu.isEndOfProgram() && instNum < states.size()) {
        // Wait until the operation is 1 cycle in so that the addressing mode and operation
        // functions have been called once. This makes it easy to know which addressing mode and
        // operation the opcode is for
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
    // $0003 if any invalid/unofficial opcodes fail, so they get printed out as well if the test
    // fails
    uint8_t testResult = cpu.readRAM(2);
    if (testResult != 0) {
        std::cout << "Failed nestest.nes valid opcodes: 0x" << std::hex << (unsigned int) testResult
            << std::dec << "\n";
        passed = false;
    }
    testResult = cpu.readRAM(3);
    if (testResult != 0) {
        std::cout << "Failed nestest/nestest.nes invalid opcodes: 0x" << std::hex <<
            (unsigned int) testResult << std::dec << "\n";
        passed = false;
    }
    if (passed) {
        std::cout << "Passed nestest/nestest.nes\n";
    }

    runIndividualTest(cpu, "official_only.nes", "instr_test-v5/", 0xec5c, 0, 0x6000);

    // cpu_timing_test.nes doesn't store the test results anywhere as far as I know, so instead the
    // while loop waits until the CPU reaches the "passed test" or the "failed test" branch in the
    // program
    cpu.clear();
    cpu.readInINES("test/cpu_timing_test6/cpu_timing_test.nes");
    uint16_t passedPC = 0xe1b7;
    uint16_t failedPC = 0xe0b0;
    while (cpu.getPC() != passedPC && cpu.getPC() != failedPC) {
        cpu.step(nullptr, nullptr);
    }

    if (cpu.getPC() == passedPC) {
        std::cout << "Passed cpu_timing_test6/cpu_timing_test.nes\n";
    } else {
        std::cout << "Failed cpu_timing_test6/cpu_timing_test.nes\n";
    }

    std::string branchDir = "branch_timing_tests/";
    runIndividualTest(cpu, "1.Branch_Basics.nes", branchDir, 0xe4f0, 1, 0xf8);
    runIndividualTest(cpu, "2.Backward_Branch.nes", branchDir, 0xe4f0, 1, 0xf8);
    runIndividualTest(cpu, "3.Forward_Branch.nes", branchDir, 0xe4f0, 1, 0xf8);
}

// Runs tests that are focused on the PPU

void runPPUTests(CPU& cpu) {
    std::string nmiDir1 = "vbl_nmi_timing/";
    uint16_t zeroPageAddr = 0xf8;
    runIndividualTest(cpu, "1.frame_basics.nes", nmiDir1, 0xe589, 1, zeroPageAddr);
    runIndividualTest(cpu, "2.vbl_timing.nes", nmiDir1, 0xe54f, 1, zeroPageAddr);
    runIndividualTest(cpu, "3.even_odd_frames.nes", nmiDir1, 0xe59f, 1, zeroPageAddr);
    runIndividualTest(cpu, "4.vbl_clear_timing.nes", nmiDir1, 0xe535, 1, zeroPageAddr);
    runIndividualTest(cpu, "5.nmi_suppression.nes", nmiDir1, 0xe54c, 1, zeroPageAddr);
    runIndividualTest(cpu, "6.nmi_disable.nes", nmiDir1, 0xe535, 1, zeroPageAddr);
    runIndividualTest(cpu, "7.nmi_timing.nes", nmiDir1, 0xe58e, 1, zeroPageAddr);

    std::string nmiDir2 = "ppu_vbl_nmi/rom_singles/";
    uint16_t prgRAMAddr = 0x6000;
    runIndividualTest(cpu, "01-vbl_basics.nes", nmiDir2, 0xe8d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "02-vbl_set_time.nes", nmiDir2, 0xe8d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "03-vbl_clear_time.nes", nmiDir2, 0xe8d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "04-nmi_control.nes", nmiDir2, 0xe8d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "05-nmi_timing.nes", nmiDir2, 0xe8d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "06-suppression.nes", nmiDir2, 0xe8d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "07-nmi_on_timing.nes", nmiDir2, 0xe8d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "08-nmi_off_timing.nes", nmiDir2, 0xe8d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "09-even_odd_frames.nes", nmiDir2, 0xe8d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "10-even_odd_timing.nes", nmiDir2, 0xead5, 0, prgRAMAddr);

    std::string spriteHitDir1 = "sprite_hit_tests_2005.10.05/";
    runIndividualTest(cpu, "01.basics.nes", spriteHitDir1, 0xe635, 1, zeroPageAddr);
    runIndividualTest(cpu, "02.alignment.nes", spriteHitDir1, 0xe635, 1, zeroPageAddr);
    runIndividualTest(cpu, "03.corners.nes", spriteHitDir1, 0xe635, 1, zeroPageAddr);
    runIndividualTest(cpu, "04.flip.nes", spriteHitDir1, 0xe5b6, 1, zeroPageAddr);
    runIndividualTest(cpu, "05.left_clip.nes", spriteHitDir1, 0xe635, 1, zeroPageAddr);
    runIndividualTest(cpu, "06.right_edge.nes", spriteHitDir1, 0xe635, 1, zeroPageAddr);
    runIndividualTest(cpu, "07.screen_bottom.nes", spriteHitDir1, 0xe635, 1, zeroPageAddr);
    runIndividualTest(cpu, "08.double_height.nes", spriteHitDir1, 0xe635, 1, zeroPageAddr);
    runIndividualTest(cpu, "09.timing_basics.nes", spriteHitDir1, 0xe64c, 1, zeroPageAddr);
    runIndividualTest(cpu, "10.timing_order.nes", spriteHitDir1, 0xe635, 1, zeroPageAddr);
    runIndividualTest(cpu, "11.edge_timing.nes", spriteHitDir1, 0xe635, 1, zeroPageAddr);

    std::string spriteHitDir2 = "ppu_sprite_hit/rom_singles/";
    runIndividualTest(cpu, "01-basics.nes", spriteHitDir2, 0xe8d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "02-alignment.nes", spriteHitDir2, 0xe8d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "03-corners.nes", spriteHitDir2, 0xe8d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "04-flip.nes", spriteHitDir2, 0xe7d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "05-left_clip.nes", spriteHitDir2, 0xe8d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "06-right_edge.nes", spriteHitDir2, 0xe8d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "07-screen_bottom.nes", spriteHitDir2, 0xe8d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "08-double_height.nes", spriteHitDir2, 0xe8d5, 0, prgRAMAddr);
    runIndividualTest(cpu, "09-timing.nes", spriteHitDir2, 0xebd5, 0, prgRAMAddr);
    runIndividualTest(cpu, "10-timing_order.nes", spriteHitDir2, 0xead5, 0, prgRAMAddr);
}

// Runs an individual .NES test until the CPU reaches the specified PC to stop at, then compares the
// test result with the known passed value to determine whether the test passed or not

void runIndividualTest(CPU& cpu, const std::string& testName, const std::string& testDirectory,
        uint16_t stopPC, uint8_t passedTestResult, uint16_t testResultAddr) {
    cpu.clear();
    cpu.readInINES("test/" + testDirectory + testName);
    while (cpu.getPC() != stopPC) {
        cpu.step(nullptr, nullptr);
    }

    uint8_t testResult;
    if (testResultAddr < 0x2000) {
        testResult = cpu.readRAM(testResultAddr);
    } else if (testResultAddr >= 0x4020) {
        testResult = cpu.readPRG(testResultAddr);
    } else {
        std::cerr << "Invalid test result address\n";
        exit(1);
    }

    if (testResult == passedTestResult) {
        std::cout << "Passed " << testDirectory << testName << "\n";
    } else {
        std::cout << "Failed " << testDirectory << testName << ": 0x" << std::hex <<
            (unsigned int) testResult << std::dec << "\n";
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

        // Listen for keypresses and pass them off to the I/O class
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