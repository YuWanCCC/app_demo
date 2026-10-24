#pragma once
#include <vector>
#include <mutex>
#include <queue>
#include <cstdint>
#include <memory>
#include <cstring>

class FrameBuffer {
public:
    FrameBuffer(size_t size) : data_size(0) {
        data = new uint8_t[size];
        capacity = size;
    }

    ~FrameBuffer() {
        delete[] data;
    }

    void reset() {
        data_size = 0;
    }

    uint8_t* data;     // 指向数据指针
    size_t capacity;   // 分配容量
    size_t data_size;  // 实际使用大小
};

class FrameBufferPool {
public:
    FrameBufferPool(size_t buffer_size, size_t count)
        : buffer_size_(buffer_size) {
        for (size_t i = 0; i < count; ++i) {
            pool_.push(std::make_unique<FrameBuffer>(buffer_size_));
        }
    }

    // 分配一个 buffer
    std::unique_ptr<FrameBuffer> Alloc() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pool_.empty()) {
            // 若没有可用，返回 nullptr（也可选择动态扩展）
            return nullptr;
        }
        auto buf = std::move(pool_.front());
        pool_.pop();
        return buf;
    }

    // 回收 buffer
    void Free(std::unique_ptr<FrameBuffer> buf) {
        std::lock_guard<std::mutex> lock(mutex_);
        buf->reset();
        pool_.push(std::move(buf));
    }

    size_t AvailableCount() {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

private:
    size_t buffer_size_;
    std::queue<std::unique_ptr<FrameBuffer>> pool_;
    std::mutex mutex_;
};
