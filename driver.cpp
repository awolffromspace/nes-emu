#include "cpu.h"

#include <fstream>
#include <string>

bool readInInst(std::string filename, uint8_t data[], unsigned int &instNum);

uint8_t hex2Dec(int hexNum);

int main(int argc, char* argv[]) {
	uint8_t data[65536] = {0};
	unsigned int instNum = 0;
	bool result = readInInst(argv[1], data, instNum);
	if (!result) {
		std::cout << "Error reading in file" << std::endl;
		exit(1);
	}
	CPU cpu(data);
	std::string input;
	std::cin >> input;
	while (true) {
		if (input == "c" || input == "continue") {
			if (cpu.getTotalInst() < instNum) {
				cpu.print(false);
				cpu.step();
				cpu.print(true);
				std::cout << "\n";
			} else {
				std::cout << std::endl;
				break;
			}
		} else if (input == "s" || input == "step") {
			cpu.print(false);
			cpu.step();
			cpu.print(true);
			input = "";
			std::cin >> input;
		} else {
			std::cout << std::endl;
			break;
		}
	}
	return 0;
}

bool readInInst(std::string filename, uint8_t data[], unsigned int &instNum) {
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
				data[dataIndex] = hex2Dec(stoi(substring));
				dataIndex++;
				substring = "";
			}
			if (i == line.size() - 1) {
				++instNum;
			}
		}
	}
	return true;
}

uint8_t hex2Dec(int hexNum) {
	uint8_t decNum = 0;
	unsigned int powOfSixteen = 1;
	for (int i = 0; i < 2; ++i) {
		decNum += (hexNum % 10) * powOfSixteen;
		powOfSixteen *= 16;
		hexNum /= 10;
	}
	return decNum;
}