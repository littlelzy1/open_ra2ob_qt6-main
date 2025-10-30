#ifndef RA2OB_SRC_READER_HPP_
#define RA2OB_SRC_READER_HPP_

#include <Windows.h>

#include <memory>
#include <string>
#include <iostream>

#include "./Constants.hpp"

namespace Ra2ob {

class Reader {
public:
    explicit Reader(HANDLE handle = nullptr);

    HANDLE getHandle();
    bool readMemory(uint32_t addr, void* value, uint32_t size);
    bool isValidAddress(uint32_t addr);
    uint32_t getAddr(uint32_t offset);
    int getInt(uint32_t offset);
    bool getBool(uint32_t offset);
    std::string getString(uint32_t offset);
    uint32_t getColor(uint32_t offset);

protected:
    HANDLE m_handle;
    static const int MAX_RETRY_COUNT = 2; // 减少重试次数
    static const DWORD READ_TIMEOUT_MS = 50; // 减少超时时间到50毫秒
    
    // 添加进程状态缓存，避免重复检查
    bool isLastCheckValid = true;
    DWORD lastCheckTime = 0;
    static const DWORD CHECK_INTERVAL_MS = 50; // 状态检查间隔时间
    
    // 检查进程是否仍在运行
    bool isProcessAlive();
};

inline Reader::Reader(HANDLE handle) { m_handle = handle; }

inline HANDLE Reader::getHandle() { return m_handle; }

// 检查进程是否仍然存活
inline bool Reader::isProcessAlive() {
    if (m_handle == nullptr) return false;
    
    DWORD currentTime = GetTickCount();
    if (currentTime - lastCheckTime < CHECK_INTERVAL_MS) {
        return isLastCheckValid;
    }
    
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(m_handle, &exitCode) || exitCode != STILL_ACTIVE) {
        isLastCheckValid = false;
    } else {
        isLastCheckValid = true;
    }
    
    lastCheckTime = currentTime;
    return isLastCheckValid;
}

inline bool Reader::isValidAddress(uint32_t addr) {
    if (addr == 0 || m_handle == nullptr || !isProcessAlive())
        return false;
        
    // 使用简化的检查方法，减少VirtualQueryEx调用
    // 对于一些常见地址范围，可以考虑缓存检查结果
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQueryEx(m_handle, (LPCVOID)(uintptr_t)addr, &mbi, sizeof(mbi))) {
        return (mbi.State == MEM_COMMIT && 
                (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)));
    }
    return false;
}

inline bool Reader::readMemory(uint32_t addr, void* value, uint32_t size) {
    // 快速检查进程状态和地址有效性
    if (!isProcessAlive() || !isValidAddress(addr)) {
        return false;
    }
    
    SIZE_T bytesRead = 0;
    BOOL result = ReadProcessMemory(m_handle, (const void*)(uintptr_t)addr, value, size, &bytesRead);
    
    if (!result || bytesRead != size) {
        // 减少错误日志输出频率，只在调试模式或特定条件下输出
        DWORD error = GetLastError();
        if (error != 0 && error != ERROR_PARTIAL_COPY) {
            // std::cerr << "Memory read failed at address 0x" << std::hex << addr 
            //           << " Error: " << std::dec << error << std::endl;
        }
        return false;
    }
    
    return true;
}

inline uint32_t Reader::getAddr(uint32_t offset) {
    // 如果进程已经不存在，直接返回无效值
    if (!isProcessAlive()) return 1;
    
    uint32_t buf = 0;
    int retryCount = 0;
    
    while (retryCount < MAX_RETRY_COUNT) {
        if (readMemory(offset, &buf, 4)) {
            return buf;
        }
        retryCount++;
        Sleep(1); // 极短延迟后重试
    }
    
    return 1; // 返回安全的默认值
}

inline int Reader::getInt(uint32_t offset) {
    // 如果进程已经不存在，直接返回无效值
    if (!isProcessAlive()) return -1;
    
    int buf = 0;
    int retryCount = 0;
    
    while (retryCount < MAX_RETRY_COUNT) {
        if (readMemory(offset, &buf, 4)) {
            return buf;
        }
        retryCount++;
        Sleep(1);
    }
    
    return -1;
}

inline bool Reader::getBool(uint32_t offset) {
    // 如果进程已经不存在，直接返回无效值
    if (!isProcessAlive()) return false;
    
    bool buf = false;
    int retryCount = 0;
    
    while (retryCount < MAX_RETRY_COUNT) {
        if (readMemory(offset, &buf, 1)) {
            return buf;
        }
        retryCount++;
        Sleep(1);
    }
    
    return false;
}

inline std::string Reader::getString(uint32_t offset) {
    // 如果进程已经不存在，直接返回空字符串
    if (!isProcessAlive()) return "";
    
    char buf[STRUNITNAMESIZE] = "\0";
    int retryCount = 0;
    
    while (retryCount < MAX_RETRY_COUNT) {
        if (readMemory(offset, &buf, STRUNITNAMESIZE)) {
            return std::string(buf);
        }
        retryCount++;
        Sleep(1);
    }
    
    return "";
}

inline uint32_t Reader::getColor(uint32_t offset) {
    // 如果进程已经不存在，直接返回无效值
    if (!isProcessAlive()) return 0;
    
    uint32_t buf = 0;
    int retryCount = 0;
    
    while (retryCount < MAX_RETRY_COUNT) {
        if (readMemory(offset, &buf, 3)) {
            return buf;
        }
        retryCount++;
        Sleep(1);
    }
    
    return 0;
}

}  // end of namespace Ra2ob

#endif  // RA2OB_SRC_READER_HPP_
