#include "cpu.h"

int main(int argc, char* argv[]) {
    CPU cpu(argv[1]);
    std::string input;
    std::cin >> input;
    while (true) {
        if (input == "c" || input == "continue") {
            cpu.step();
            std::cout << "\n";
        } else if (input == "s" || input == "step") {
            cpu.step();
            input = "";
            std::cin >> input;
        } else {
            break;
        }
    }
    return 0;
}