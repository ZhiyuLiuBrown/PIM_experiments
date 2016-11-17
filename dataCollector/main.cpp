/*
 * main.cpp
 *
 *  Created on: Aug 12, 2016
 *      Author: zhiyul
 */
#include <iostream>
#include <string>
#include <fstream>
#include <stdlib.h>

int main() {
	using namespace std;

	ifstream file("dataFile");
	if (file.is_open()) {
		int results[5][10][65];

		for (int j = 0; j < 5; j++)
			for (int i = 0; i < 10; i++)
				for (int numProcs = 1; numProcs <= 64; numProcs++)
					results[j][i][numProcs] = 0;

		int num_iter = 3;
		string algs[6] = { "single FC", "lock free", "FC with 2 partitions",
				"FC with 4 partitions", "FC with 8 partitions",
				"FC with 16 partitions" };

		for (int iter = 0; iter < num_iter; iter++) {
			for (int j = 0; j <= 2; j++) //0-2
				for (int i = 0; i <= 5; i = i + 1) //0-5; #partitions: 2->2, 3->4, 4->8, 5->16
					for (int numProcs = 1; numProcs <= 64; numProcs = numProcs + 3) {

						if (i == 1)
							i = 2;

						string str;

						do {
							file >> str;
						} while (str.compare("#operations:"));

						file >> str;

						int num = atoi(str.c_str());

						if (iter != 0)
							results[j][i][numProcs] = results[j][i][numProcs] + num;
					}

		}

		for (int j = 0; j <= 2; j++)
			for (int i = 0; i <= 5; i++)
				for (int numProcs = 1; numProcs <= 64; numProcs = numProcs + 3) {
					if (numProcs == 1)
						std::cerr << "********************\n";
					std::cerr << results[j][i][numProcs] / (num_iter - 1) << "\n";
				}
	}

	return 0;
}



