#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

class StartGate {
public:
    StartGate() : open_(false) {}

    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!open_) {
            cv_.wait(lock);
        }
    }

    void open() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            open_ = true;
        }
        cv_.notify_all();
    }

private:
    bool open_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

class Account {
public:
    explicit Account(int opening_balance)
        : balance_(opening_balance) {}

    bool withdraw(int amount) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (balance_ < amount) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        balance_ -= amount;
        return true;
    }

    int balance() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return balance_;
    }

private:
    int balance_;
    mutable std::mutex mutex_;
};

int main() {
    Account account(100);
    StartGate gate;
    std::atomic<int> approvals(0);

    std::thread worker1([&]() {
        gate.wait();
        if (account.withdraw(60)) {
            approvals.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::thread worker2([&]() {
        gate.wait();
        if (account.withdraw(60)) {
            approvals.fetch_add(1, std::memory_order_relaxed);
        }
    });

    gate.open();
    worker1.join();
    worker2.join();

    std::cout << "approved_withdrawals=" << approvals.load()
              << " final_balance=" << account.balance() << "\n";

    return approvals.load() == 1 ? 0 : 2;
}
