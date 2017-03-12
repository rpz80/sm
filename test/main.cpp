#define CATCH_CONFIG_MAIN
#include <mutex>
#include <thread>
#include <memory>
#include <chrono>
#include <sm/shared_mutex.h>
#include "catch.hh"

class Reader {
public:
  Reader(sm::shared_mutex& mutex) : 
    m_mutex(mutex) {}

  void read() {
    m_mutex.lock_shared();
  }

  ~Reader() {
    m_mutex.unlock_shared();
  }

  template<typename F>
  static std::thread start(sm::shared_mutex& mutex, F f) {
    return std::thread(
        [f, reader = std::unique_ptr<Reader>(new Reader(mutex))]() mutable {
          f(std::move(reader));
        });
  }

private:
  sm::shared_mutex& m_mutex;
};

class Writer {
public:
private:
};

class TestMutex : public sm::shared_mutex {
public:
  size_t readerCount() const {
    return sm::shared_mutex::readerCount();
  }
};

template<typename F>
void runReaders(size_t readerCount, TestMutex& mutex, F f) {
  std::vector<std::thread> readers;

  for (size_t i = 0; i < readerCount; ++i) {
    readers.emplace_back(
        Reader::start(mutex, [](std::unique_ptr<Reader> r) {
          r->read();
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }));
  }

  f();

  for (size_t i = 0; i < readerCount; ++i) {
    readers[i].join();
  }
}

TEST_CASE("SharedMutex") {
  TestMutex mutex;
  const size_t kReaderCount = 42;

  SECTION("Multiple reads") {
    runReaders(kReaderCount, mutex, [&mutex, kReaderCount]() {
      REQUIRE(mutex.try_lock_shared() == true);
      REQUIRE(mutex.try_lock() == false);
      mutex.unlock_shared();

      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      REQUIRE(mutex.readerCount() == kReaderCount);
    });

    REQUIRE(mutex.readerCount() == 0);
    REQUIRE(mutex.try_lock());
  }

  SECTION("Try write lock while read locks are taken") {
    runReaders(kReaderCount, mutex, [&mutex]() {
      auto start = std::chrono::steady_clock::now();
      mutex.lock();
      auto end = std::chrono::steady_clock::now();
      REQUIRE(std::chrono::duration_cast
                  <std::chrono::milliseconds>(
                      end - start).count() >= 100);
      mutex.unlock();
    });
  }

  SECTION("Try write lock while write lock is taken") {
    std::thread([&mutex]() {
      mutex.lock();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      mutex.unlock();
    }).detach();

      REQUIRE(mutex.try_lock() == false);
      REQUIRE(mutex.try_lock_shared() == false);

      auto start = std::chrono::steady_clock::now();
      mutex.lock();
      auto end = std::chrono::steady_clock::now();
      REQUIRE(std::chrono::duration_cast
                  <std::chrono::milliseconds>(
                      end - start).count() >= 100);
  }

  SECTION("Try read lock while write lock is taken") {
    std::thread([&mutex]() {
      mutex.lock();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      mutex.unlock();
    }).detach();

      REQUIRE(mutex.try_lock() == false);
      REQUIRE(mutex.try_lock_shared() == false);

      auto start = std::chrono::steady_clock::now();
      mutex.lock_shared();
      auto end = std::chrono::steady_clock::now();
      REQUIRE(std::chrono::duration_cast
                  <std::chrono::milliseconds>(
                      end - start).count() >= 100);
  }
}
