#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <random>
#include <string>

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

void processWithMutex(const vector<int>& dataVector, size_t startIndex, size_t endIndex,
    long& sharedXorResult, mutex& sharedMutex) {
    for (size_t i = startIndex; i < endIndex; ++i) {
        if (dataVector[i] % 7 == 0) {
            lock_guard<mutex> lock(sharedMutex);
            sharedXorResult ^= dataVector[i];
        }
    }
}

long findXorWithMutex(const vector<int>& dataVector, int totalThreads) {
    long globalXorResult = 0;
    mutex resultMutex;
    vector<thread> workerThreads;

    size_t partitionSize = dataVector.size() / totalThreads;

    for (int t = 0; t < totalThreads; ++t) {
        size_t startIndex = t * partitionSize;
        size_t endIndex = (t == totalThreads - 1) ? dataVector.size() : startIndex + partitionSize;

        workerThreads.emplace_back(processWithMutex,
            cref(dataVector), startIndex, endIndex,
            ref(globalXorResult), ref(resultMutex));
    }

    for (auto& currentThread : workerThreads)
        currentThread.join();

    return globalXorResult;
}

void processWithCAS(const vector<int>& dataVector, size_t startIndex, size_t endIndex,
    atomic<long>& sharedAtomicResult) {
    for (size_t i = startIndex; i < endIndex; ++i) {
        if (dataVector[i] % 7 == 0) {
            long expectedValue, desiredValue;

            do {
                expectedValue = sharedAtomicResult.load();
                desiredValue = expectedValue ^ dataVector[i];
            } while (!sharedAtomicResult.compare_exchange_strong(expectedValue, desiredValue));
        }
    }
}

long findXorWithCAS(const vector<int>& dataVector, int totalThreads) {
    atomic<long> globalAtomicResult(0);
    vector<thread> workerThreads;

    size_t partitionSize = dataVector.size() / totalThreads;

    for (int t = 0; t < totalThreads; ++t) {
        size_t startIndex = t * partitionSize;
        size_t endIndex = (t == totalThreads - 1) ? dataVector.size() : startIndex + partitionSize;

        workerThreads.emplace_back(processWithCAS,
            cref(dataVector), startIndex, endIndex,
            ref(globalAtomicResult));
    }

    for (auto& currentThread : workerThreads)
        currentThread.join();

    return globalAtomicResult.load();
}

int main() {
    vector<size_t> dataSizes = { 100000, 500000, 1000000, 2000000 };
    vector<int> threadOptions = { 1, 4, 6, 10, 12, 16 };

    cout << left
        << setw(10) << "Size"
        << setw(6) << "Thr"
        << setw(10) << "Simple"
        << setw(10) << "Mutex"
        << setw(10) << "CAS"
        << " | Check\n";

    cout << string(55, '-') << endl;

    using clock_type = chrono::high_resolution_clock;

    for (size_t currentSize : dataSizes) {
        vector<int> valuesVector(currentSize);
        fillRandomData(valuesVector);

        auto startTimeSequential = clock_type::now();
        long baseResult = findXor(valuesVector);
        auto endTimeSequential = clock_type::now();

        double durationSimple = chrono::duration<double, milli>(endTimeSequential - startTimeSequential).count();

        for (int threadsCount : threadOptions) {
            auto startTimeMutex = clock_type::now();
            long mutexResult = findXorWithMutex(valuesVector, threadsCount);
            auto endTimeMutex = clock_type::now();

            double durationMutex = chrono::duration<double, milli>(endTimeMutex - startTimeMutex).count();

            auto startTimeCAS = clock_type::now();
            long casResult = findXorWithCAS(valuesVector, threadsCount);
            auto endTimeCAS = clock_type::now();

            double durationCAS = chrono::duration<double, milli>(endTimeCAS - startTimeCAS).count();

            bool isCorrect = (baseResult == mutexResult && baseResult == casResult);

            cout << left << setw(10) << currentSize
                << setw(6) << threadsCount
                << fixed << setprecision(2);

            if (threadsCount == 1)
                cout << setw(10) << durationSimple;
            else
                cout << setw(10) << "-";

            cout << setw(10) << durationMutex
                << setw(10) << durationCAS
                << " | " << (isCorrect ? "Correct" : "Fail") << endl;
        }

        cout << string(55, '=') << endl;
    }

    return 0;
}