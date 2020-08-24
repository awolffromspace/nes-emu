#include "cpu.h"

#include <fstream>
#include <string>

bool readInInst(std::string filename, uint8_t data[]);

int main(int argc, char* argv[]) {
	uint8_t data[65536] = {0};
	bool result = readInInst(argv[1], data);
	if (!result) {
		std::cout << "Error reading in file" << std::endl;
		exit(1);
	}
	CPU cpu(data);
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

bool readInInst(std::string filename, uint8_t data[]) {
	std::string line;
	std::ifstream file(filename.c_str());
	if (!file.is_open()) {
		return false;
	}
	int dataIndex = 0x8000;
	while (file.good()) {
		getline(file, line);
		std::string substring = "";
		for (int i = 0; i < line.size(); ++i) {
			substring += line.at(i);
			if (i % 2 == 1) {
				data[dataIndex] = std::stoul(substring, nullptr, 16);
				dataIndex++;
				substring = "";
			}
		}
	}
	return true;
}