#include <cstddef>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <queue>
#include <sys/types.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <type_traits>
#include <utility>
#include <vector>


class ThreadPool {
private:
    std::vector<std::thread> workers;

    std::atomic<bool> stop;

    std::condition_variable cv;

    std::mutex queMutes;
    std::queue<std::function<void()>> task;

public:
    explicit ThreadPool(size_t countThread) : stop(false) {
        // constructor
        for (size_t i = 0; i < countThread; i++) {
            workers.emplace_back([this] {
                for(;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queMutes);

                        cv.wait(lock, [this] {
                            return (this -> stop || !(this -> task.empty()));
                        });

                        if (stop && this -> task.empty()) return;
                        task = std::move(this -> task.front());
                        this -> task.pop();
                    }
                    task();
                }
            });
        }
    }


    template<class F, class... Args>    
    auto enqueue (F&& f, Args&&... args) -> std::future<std::invoke_result_t<F , Args...>> {
        try {
            // add in task queue
            using return_type = std::invoke_result_t<F, Args...>;

            auto take_result = std::make_shared<std::packaged_task<return_type()>> (std::bind(std::forward<F>(f),  std::forward<Args>(args)...));

            std::future<return_type> res = take_result -> get_future();

            {
                std::unique_lock<std::mutex> lock(queMutes);

                if (stop) {
                    throw std::runtime_error("enqueue on stopped ThreadPool");
                }

                task.emplace([take_result]() {
                    (*take_result)();
                });
            }

            cv.notify_one();
            return res;
        }
        catch (const std::exception& exception) {
            throw std::runtime_error(std::string("ThreadPool enqueue failed: ") + exception.what());;
        }
    }

    ~ThreadPool() {
        // deconstructor
        {
            std::unique_lock<std::mutex> lock(queMutes);
            stop = true;
        }

        cv.notify_all(); // что бы потоки проверили все состояния

        for(std::thread& worker : workers) {
            worker.join();
        }

    }
};