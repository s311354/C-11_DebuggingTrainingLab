#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>

class Account {
public:
    explicit Account(int opening_balance)
        : balance_(opening_balance) {}

    int read_balance_for_approval() const {
        return balance_;
    }

    bool commit_withdrawal(int observed, int amount) {
        if (observed < amount) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        balance_ = observed - amount;
        return true;
    }

    int balance() const {
        return balance_;
    }

private:
    int balance_;
};

static void worker(Account& account,
                   int amount,
                   std::promise<int> observed_balance,
                   std::shared_future<void> commit_now,
                   std::atomic<int>& approvals) {
    int observed = account.read_balance_for_approval();
    observed_balance.set_value(observed);

    commit_now.wait();

    if (account.commit_withdrawal(observed, amount)) {
        approvals.fetch_add(1, std::memory_order_relaxed);
    }
}

int main() {
    Account account(100);
    std::atomic<int> approvals(0);

    std::promise<int> observed1;
    std::promise<int> observed2;
    std::future<int> observed1_done = observed1.get_future();
    std::future<int> observed2_done = observed2.get_future();

    std::promise<void> commit_signal;
    std::shared_future<void> commit_now(commit_signal.get_future());

    std::thread worker1(worker,
                        std::ref(account),
                        60,
                        std::move(observed1),
                        commit_now,
                        std::ref(approvals));
    std::thread worker2(worker,
                        std::ref(account),
                        60,
                        std::move(observed2),
                        commit_now,
                        std::ref(approvals));

    int first_observed = observed1_done.get();
    int second_observed = observed2_done.get();
    std::cout << "observed_balances=" << first_observed << ","
              << second_observed << "\n";

    commit_signal.set_value();

    worker1.join();
    worker2.join();

    std::cout << "approved_withdrawals=" << approvals.load()
              << " final_balance=" << account.balance() << "\n";

    if (approvals.load() == 2) {
        std::cerr << "BUG: two withdrawals were approved from a 100-unit account\n";
        return 2;
    }

    return 0;
}
