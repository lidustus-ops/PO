#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>

using namespace std;

void fillRandomData(vector<int>& dataVector) {
    for (int& element : dataVector)
        element = rand() % 1000 + 1;
}

long findXor(const vector<int>& dataVector) {
    long xorResult = 0;

    for (int value : dataVector) {
        if (value % 7 == 0)
            xorResult ^= value;
    }

    return xorResult;
}

int main() {
    srand(time(0));

    size_t currentSize = 100000;
    vector<int> valuesVector(currentSize);

    fillRandomData(valuesVector);

    long result = findXor(valuesVector);

    cout << "Size: " << currentSize << endl;
    cout << "Result: " << result << endl;

    return 0;
}