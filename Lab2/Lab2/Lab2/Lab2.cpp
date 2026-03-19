#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <random>

using namespace std;

void fillRandomData(vector<int>& dataVector) {
    static mt19937 generator(42);
    uniform_int_distribution<int> distribution(1, 1000);

    for (int& element : dataVector)
        element = distribution(generator);
}

long findXor(const vector<int>& dataVector) {
    long xorResult = 0;

    for (int value : dataVector) {
        if (value % 7 == 0)
            xorResult ^= value;
    }

    return xorResult;
}

void processOptimized(const vector<int>& dataVector, size_t startIndex, size_t endIndex,
    long& sharedXorResult, mutex& sharedMutex) {

    long localXor = 0;

    for (size_t i = startIndex; i < endIndex; ++i) {
        if (dataVector[i] % 7 == 0) {
            localXor ^= dataVector[i];
        }
    }

    lock_guard<mutex> lock(sharedMutex);
    sharedXorResult ^= localXor;
}

long findXorWithMutex(const vector<int>& dataVector, int totalThreads) {
    long globalXorResult = 0;
    mutex resultMutex;
    vector<thread> workerThreads;

    size_t partitionSize = dataVector.size() / totalThreads;

    for (int t = 0; t < totalThreads; ++t) {
        size_t startIndex = t * partitionSize;
        size_t endIndex = (t == totalThreads - 1)
            ? dataVector.size()
            : startIndex + partitionSize;

        workerThreads.emplace_back(processOptimized,
            cref(dataVector), startIndex, endIndex,
            ref(globalXorResult), ref(resultMutex));
    }

    for (auto& th : workerThreads)
        th.join();

    return globalXorResult;
}

int main() {
    vector<size_t> dataSizes = { 100000, 500000, 1000000, 2000000 };
    vector<int> threadOptions = { 1, 4, 6, 10, 12, 16 };

    cout << left
        << setw(10) << "Size"
        << setw(6) << "Thr"
        << setw(10) << "Simple"
        << setw(10) << "Mutex"
        << " | Check\n";

    cout << string(45, '-') << endl;

    using clock_type = chrono::high_resolution_clock;

    for (size_t currentSize : dataSizes) {
        vector<int> valuesVector(currentSize);
        fillRandomData(valuesVector);

        auto startTimeSequential = clock_type::now();
        long baseResult = findXor(valuesVector);
        auto endTimeSequential = clock_type::now();

        double durationSimple = chrono::duration<double, milli>(
            endTimeSequential - startTimeSequential).count();

        for (int threadsCount : threadOptions) {
            auto startTimeMutex = clock_type::now();
            long mutexResult = findXorWithMutex(valuesVector, threadsCount);
            auto endTimeMutex = clock_type::now();

            double durationMutex = chrono::duration<double, milli>(
                endTimeMutex - startTimeMutex).count();

            bool isCorrect = (baseResult == mutexResult);

            cout << left << setw(10) << currentSize
                << setw(6) << threadsCount
                << fixed << setprecision(2);

            if (threadsCount == 1)
                cout << setw(10) << durationSimple;
            else
                cout << setw(10) << "-";

            cout << setw(10) << durationMutex
                << " | " << (isCorrect ? "Correct" : "Fail") << endl;
        }

        cout << string(45, '=') << endl;
    }

    return 0;
}