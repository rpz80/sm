#pragma once

#include <mutex>
#include <condition_variable>

namespace sm {
class shared_mutex {
public:
  shared_mutex() : m_readerCount(0),
                   m_writeInProgress(false) {}

  shared_mutex(const shared_mutex&) = delete;
  shared_mutex& operator=(const shared_mutex&) = delete;

  void lock() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_writeCond.wait(lock, [this]() {
      return !m_writeInProgress;
    });
    m_writeInProgress = true;
    m_readCond.wait(lock, [this]() {
      return m_readerCount == 0;
    });
  }
  
  bool try_lock() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_writeInProgress && m_readerCount == 0) {
      m_writeInProgress = true;
      return true;
    }
    return false;
  }

  void unlock() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_writeInProgress = false;
    m_writeCond.notify_one();
    m_readCond.notify_one();
  }

  void lock_shared() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_readCond.wait(lock, [this]() {
      return m_writeInProgress == false;
    });
    ++m_readerCount;
  }

  bool try_lock_shared() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_writeInProgress) {
      ++m_readerCount;
      return true;
    }
    return false;
  }

  void unlock_shared() {
    std::lock_guard<std::mutex> lock(m_mutex);
    --m_readerCount;
    m_readCond.notify_all();
  }

protected:
  size_t readerCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_readerCount;
  }

private:
  mutable std::mutex m_mutex;
  std::condition_variable m_writeCond;
  std::condition_variable m_readCond;
  size_t m_readerCount;
  bool m_writeInProgress;
};

template<typename LockType>
class shared_lock {
public:
  shared_lock(LockType& lock) : m_lock(lock) {
    m_lock.lock_shared();
  }

  ~shared_lock() {
    m_lock.unlock_shared();
  }

private:
  LockType& m_lock;
};
}
