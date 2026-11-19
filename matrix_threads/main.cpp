#include <QThread>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

using Matrix = std::vector<double>;

// Кожен потік обчислює власні рядки результату.
void multiplyRows(const Matrix &a, const Matrix &b, Matrix &c, int n, int first, int last) {
    for (int i = first; i < last; ++i)
        for (int k = 0; k < n; ++k)
            for (int j = 0; j < n; ++j)
                c[static_cast<size_t>(i) * n + j] += a[static_cast<size_t>(i) * n + k] * b[static_cast<size_t>(k) * n + j];
}

Matrix multiply(const Matrix &a, const Matrix &b, int n, int count, bool useQt) {
    Matrix c(static_cast<size_t>(n) * n, 0.0);
    std::vector<std::thread> cppThreads;
    std::vector<QThread *> qtThreads;
    for (int t = 0; t < count; ++t) {
        int first = static_cast<long long>(n) * t / count;
        int last = static_cast<long long>(n) * (t + 1) / count;
        if (useQt) {
            auto *thread = QThread::create([&, first, last] { multiplyRows(a, b, c, n, first, last); });
            qtThreads.push_back(thread);
            thread->start();
        } else {
            cppThreads.emplace_back(multiplyRows, std::cref(a), std::cref(b), std::ref(c), n, first, last);
        }
    }
    for (auto &thread : cppThreads) thread.join();
    for (auto *thread : qtThreads) { thread->wait(); delete thread; }
    return c;
}

int main(int argc, char *argv[]) {
    try {
        if (argc > 3) throw std::invalid_argument("arguments");
        auto parsePositive = [](const char *text) {
            std::string value(text);
            size_t used = 0;
            int number = std::stoi(value, &used);
            if (used != value.size() || number <= 0) throw std::invalid_argument("number");
            return number;
        };
        int n = argc > 1 ? parsePositive(argv[1]) : 500;
        unsigned int cores = std::thread::hardware_concurrency();
        int count = argc > 2 ? parsePositive(argv[2]) : static_cast<int>(cores ? cores : 1);
        count = std::min(count, n);
        Matrix a(static_cast<size_t>(n) * n), b(a.size());
        std::mt19937 generator(42);
        std::uniform_real_distribution<double> random(0.0, 1.0);
        for (double &x : a) x = random(generator);
        for (double &x : b) x = random(generator);
        auto run = [&](const char *name, int threads, bool qt) {
            auto start = std::chrono::steady_clock::now();
            Matrix result = multiply(a, b, n, threads, qt);
            auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            std::cout << name << ": " << elapsed << " с (потоків: " << threads << ")\n";
            return result;
        };
        Matrix single = run("Один потік", 1, false);
        Matrix cpp = run("C++", count, false);
        Matrix qt = run("Qt", count, true);
        double difference = 0;
        for (size_t i = 0; i < single.size(); ++i)
            difference = std::max({difference, std::abs(single[i] - cpp[i]), std::abs(single[i] - qt[i])});
        std::cout << "Найбільша різниця результатів: " << difference << '\n';
    } catch (const std::exception &e) {
        std::cerr << "Помилка: " << e.what() << "\nВикористання: matrix_threads [N] [кількість_потоків] (додатні цілі числа)\n";
        return 1;
    }
}
