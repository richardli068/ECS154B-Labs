//
//  main.cpp
//  ROM Filler
//
//  Created by Richard Li on 1/11/18.
//  Copyright © 2018 Richard Li. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <string>

using namespace std;

int main(int argc, const char * argv[]) {
  ifstream ifs(argv[1]);
  short inputBits, outputBits;
  ifs >> inputBits >> outputBits;
  vector<short> ROM;
  for(int i = 0; i < pow(2, inputBits); i++)
  {
    ROM.push_back(0);
  }
  short input, output;
  while(ifs >> input >> output)
  {
    ROM[input] = output;
  }

  ofstream ofs("ROM.txt");
  ofs << "v2.0 raw" << endl;
  for(int i = 0; i < pow(2, inputBits) / 8; i++)
  {
    for(int j = 0; j < 8; j++)
      ofs << hex << ROM[i * 8 + j] << ' ';
    ofs << endl;
  }
  return 0;
}
