#pragma once
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// 1. Conditionally include PC/C++20 headers
#ifndef ARDUINO
#include <iostream>
#endif

#if __cplusplus >= 202002L
#include <compare>
#endif

// 2. STL string checks
#if defined(__has_include)
#  if __has_include(<string>)
#    include <string>
#    define HAS_STL_STRING
#  endif
#  if __has_include(<string_view>)
#    include <string_view>
#    define HAS_STL_STRING_VIEW
#  endif
#endif

// 3. Arduino Checks
#if defined(ARDUINO)
#include <Arduino.h>
#endif

// 4. Macro to handle printing errors cross-platform
#if defined(ARDUINO)
#define PRINT_WARNING(msg) if(Serial) Serial.println(msg)
#else
#define PRINT_WARNING(msg) printf("%s\n", msg)
#endif

class string_view { 
protected:
    const char* m_data; 
    size_t m_len;

public:
    string_view() : m_data(nullptr), m_len(0) {}
    string_view(const char* data) : m_data(data) {
        m_len = (data) ? strlen(data) : 0;
    }
    string_view(const char* data, size_t len) : m_data(data), m_len(len) {}

    #ifdef HAS_STL_STRING
    string_view(const std::string& std_str) 
        : m_data(std_str.data()), m_len(std_str.length()) {}

    operator std::string() const {
        if (!m_data || m_len == 0) return std::string();
        return std::string(m_data, m_len);
    }
    #endif

    size_t size() const { return m_len; }
    const char* data() const { return m_data; }

    char operator[](size_t index) const {
        return m_data[index]; 
    }
    
    char at(size_t index) const {
        if (index >= m_len) {
            PRINT_WARNING("ERROR: Index out of bounds!");
            return '\0'; 
        }
        return m_data[index];
    }

    int compare(const string_view& other) const {
        size_t min_len = (m_len < other.m_len) ? m_len : other.m_len;
        
        const char* d1 = m_data ? m_data : "";
        const char* d2 = other.m_data ? other.m_data : "";
        
        int cmp = memcmp(d1, d2, min_len);
        if (cmp != 0) return cmp;
        
        if (m_len < other.m_len) return -1;
        if (m_len > other.m_len) return 1;
        return 0;
    }

    // 5. Safely handle C++20 spaceship vs C++11 standard operators
    #if __cplusplus >= 202002L
    std::strong_ordering operator<=>(const string_view& other) const {
        const char* d1 = m_data ? m_data : "";
        const char* d2 = other.m_data ? other.m_data : "";
        
        // Replaced std::min with ternary to avoid <algorithm> and macro clashes
        size_t len = (m_len < other.m_len) ? m_len : other.m_len; 
        int cmp = memcmp(d1, d2, len);
        
        if (cmp != 0) return cmp <=> 0;
        return m_len <=> other.m_len;
    }
    #else
    bool operator<(const string_view& other) const { return compare(other) < 0; }
    bool operator>(const string_view& other) const { return compare(other) > 0; }
    bool operator<=(const string_view& other) const { return compare(other) <= 0; }
    bool operator>=(const string_view& other) const { return compare(other) >= 0; }
    #endif

    bool operator==(const string_view& other) const { return compare(other) == 0; }
    bool operator!=(const string_view& other) const { return compare(other) != 0; }
    bool operator==(const char* str) const { return compare(string_view(str)) == 0; }
    bool operator!=(const char* str) const { return compare(string_view(str)) != 0; }

    int indexOf(char c) const {
        for (size_t i = 0; i < m_len; ++i) {
            if (m_data[i] == c) return static_cast<int>(i);
        }
        return -1;
    }

    bool startsWith(const string_view& prefix) const {
        if (prefix.m_len > m_len) return false;
        if (prefix.m_len == 0) return true;
        
        const char* d1 = m_data ? m_data : "";
        const char* d2 = prefix.m_data ? prefix.m_data : "";
        
        return memcmp(d1, d2, prefix.m_len) == 0;
    }

    bool startsWith(const char* prefix) const {
        return startsWith(string_view(prefix));
    }

    int find(const string_view& substr) const {
        if (substr.m_len == 0) return 0;
        if (substr.m_len > m_len) return -1;
        
        const char* d1 = m_data ? m_data : "";
        const char* d2 = substr.m_data ? substr.m_data : "";

        for (size_t i = 0; i <= m_len - substr.m_len; ++i) {
            if (memcmp(d1 + i, d2, substr.m_len) == 0) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    int find(const char* substr) const {
        return find(string_view(substr));
    }

    void print() const {
        if (m_data) {
            #if defined(ARDUINO)
            Serial.write((const uint8_t*)m_data, m_len);
            #else
            std::cout.write(m_data, m_len);
            #endif
        }
    }
};

class string : public string_view {
    protected:
        char* buffer;
        size_t capacity_; 
        bool m_owns_memory;

        void sync_view() { m_data = buffer; }

        char* append_impl(const char* str, size_t str_len) {
            size_t available_space = capacity_ - m_len;
            size_t to_copy = (str_len < available_space) ? str_len : available_space;
            
            if (str_len > available_space) {
                PRINT_WARNING("WARNING: Truncating string append.");
            }
            
            memcpy(buffer + m_len, str, to_copy);
            m_len += to_copy;
            buffer[m_len] = '\0';
            
            sync_view();
            return buffer;
        }

        string(size_t cap, char* buf)
            : string_view(buf, 0), buffer(buf), capacity_(cap), m_owns_memory(false) {
            buffer[0] = '\0';
        }

        static size_t calc_min_cap(size_t req) { return (req < 8) ? 8 : req; }

    public:
        const char *c_str() const { return buffer; };  
        char* data() { return buffer; }
        size_t capacity() const { return capacity_; };

        string(const char *cstr) 
            : string_view(nullptr, 0), capacity_(0), m_owns_memory(true) 
        {
            size_t len = (cstr) ? strlen(cstr) : 0;
            capacity_ = calc_min_cap(len);
            
            buffer = new char[capacity_ + 1];
            if (len > 0) memcpy(buffer, cstr, len);
            m_len = len;
            buffer[m_len] = '\0';
            sync_view();
        }

        string(const char *data, size_t size) 
            : string_view(nullptr, 0), capacity_(calc_min_cap(size)), m_owns_memory(true) 
        {
            buffer = new char[capacity_ + 1];
            if (size > 0 && data) memcpy(buffer, data, size);
            m_len = size;
            buffer[m_len] = '\0';
            sync_view();
        }

        string(const string_view& sv) 
            : string_view(nullptr, 0), capacity_(calc_min_cap(sv.size())), m_owns_memory(true) 
        {
            buffer = new char[capacity_ + 1];
            if (sv.size() > 0) memcpy(buffer, sv.data(), sv.size());
            m_len = sv.size();
            buffer[m_len] = '\0';
            sync_view();
        }

        string(const string& other) 
            : string_view(nullptr, 0), buffer(nullptr), capacity_(calc_min_cap(other.capacity_)), m_owns_memory(true)
        {
            buffer = new char[capacity_ + 1];    
            size_t to_copy = (other.m_len < capacity_) ? other.m_len : capacity_;
            if (to_copy > 0) memcpy(buffer, other.buffer, to_copy);
            m_len = to_copy;
            buffer[m_len] = '\0';
            sync_view();
        }

        string(string&& other) noexcept
            : string_view(nullptr, 0), buffer(nullptr), capacity_(0), m_owns_memory(false)
        {
            *this = static_cast<string&&>(other);
        }

        string& operator=(string&& other) noexcept {
            if (this != &other) {
                if (m_owns_memory && buffer) delete[] buffer;
                
                buffer = other.buffer;
                capacity_ = other.capacity_;
                m_owns_memory = other.m_owns_memory;
                m_data = buffer;
                m_len = other.m_len;

                other.buffer = nullptr;
                other.m_data = nullptr;
                other.m_len = 0;
                other.capacity_ = 0;
                other.m_owns_memory = false;
            }
            return *this;
        }

        #ifdef HAS_STL_STRING
        string(const std::string& std_str) 
        : string_view(nullptr, 0), capacity_(calc_min_cap(std_str.length())), m_owns_memory(true) 
        {
            buffer = new char[capacity_ + 1];
            if (!std_str.empty()) memcpy(buffer, std_str.data(), std_str.length());
            m_len = std_str.length();
            buffer[m_len] = '\0';
            sync_view();
        }
        operator std::string() const {
            if (!buffer || m_len == 0) return std::string();
            return std::string(buffer, m_len);
        }
        #endif

        #if defined(ARDUINO)
        string(const String& ard_str)
            : string_view(nullptr, 0), capacity_(calc_min_cap(ard_str.length())), m_owns_memory(true)
        {
            buffer = new char[capacity_ + 1];
            if (ard_str.length() > 0) memcpy(buffer, ard_str.c_str(), ard_str.length());
            m_len = ard_str.length();
            buffer[m_len] = '\0';
            sync_view();
        }
        operator String() const {
            return String(buffer);
        }
        #endif

        virtual ~string() {
            if (m_owns_memory && buffer != nullptr) {
                delete[] buffer;
            }
        }

        char& operator[](size_t index) { return buffer[index]; }

        char& at(size_t index) {
            if (index >= m_len) {
                PRINT_WARNING("ERROR: Index out of bounds!");
                return buffer[m_len > 0 ? m_len - 1 : 0]; 
            }
            return buffer[index];
        }

        string& operator=(const char* str) {
            if (str == nullptr) {
                m_len = 0; buffer[0] = '\0'; sync_view(); return *this;
            }
            
            size_t str_len = strlen(str);
            size_t to_copy = (str_len < capacity_) ? str_len : capacity_;
            
            if (buffer) {
                memcpy(buffer, str, to_copy);
                m_len = to_copy;
                buffer[m_len] = '\0';
                sync_view();
            } else {
                PRINT_WARNING("CRITICAL ERROR: Buffer is NULL in operator=");
            }
            return *this;
        }

        string& operator=(const string& other) {
            if (this != &other) {
                size_t to_copy = (other.m_len < capacity_) ? other.m_len : capacity_;
                memcpy(buffer, other.buffer, to_copy);
                m_len = to_copy;
                buffer[m_len] = '\0';
                sync_view();
            }
            return *this;
        }

        virtual bool concat(const char c) { return append_impl(&c, 1) != nullptr; }
        virtual bool concat(const char* str) { 
            if (!str) return false;
            return append_impl(str, strlen(str)) != nullptr; 
        }
        virtual bool concat(const string_view& sv) { 
            return append_impl(sv.data(), sv.size()) != nullptr; 
        }
        virtual bool concat(int num) {
            char num_str[12];
            int len = snprintf(num_str, sizeof(num_str), "%d", num);
            if (len < 0) return false;
            return concat(string_view(num_str, len)); 
        }
        virtual bool concat(float num) {
            char num_str[32];
            int len = snprintf(num_str, sizeof(num_str), "%.2f", num); 
            if (len < 0) return false;
            return concat(string_view(num_str, len));
        }

        bool operator+=(const string& other) { return concat(other); }
        
        virtual bool replace(const char* old_str, const char* new_str) {
            if (!old_str || !new_str) return false;
            int index = find(old_str);
            if (index == -1) return false;

            size_t old_len = strlen(old_str);
            size_t new_len = strlen(new_str);
            size_t new_total_len = m_len - old_len + new_len;
            
            if (new_total_len > capacity_) return false; 

            char* match_start = buffer + index;
            char* tail_start = match_start + old_len;
            size_t tail_len = m_len - (index + old_len);
            
            memmove(match_start + new_len, tail_start, tail_len);
            memcpy(match_start, new_str, new_len);
            
            m_len = new_total_len;
            buffer[m_len] = '\0';
            sync_view();      
            return true;
        }
};

inline string to_string(int num) {
    char num_str[12];
    snprintf(num_str, sizeof(num_str), "%d", num);
    return string(num_str);
}
inline string to_string(float num) {
    char num_str[32];
    snprintf(num_str, sizeof(num_str), "%.2f", num); 
    return string(num_str); 
}
inline string to_string(const char* str) { return string(str); }
inline string to_string(const char c) { char str[2] = {c, '\0'}; return string(str); }

template <size_t N>
class FixedString : public string {
    public:
        FixedString() : string(N, storage) {}
        FixedString(const FixedString &other) : string(N, storage) { 
            string::operator=(other); 
        }

        FixedString& operator=(const FixedString &other) {
            if (this != &other) {
                string::operator=(other); 
            }
            return *this;
        }

        FixedString(const char* str) : string(N, storage) {
            string::operator=(str); 
        }

        FixedString(const char* buffer, size_t len) : string(N, storage) {
            size_t to_copy = (len < N) ? len : N;
            if (len > N) {
                PRINT_WARNING("WARNING: Truncating string initialization.");
            }
            if (buffer && to_copy > 0) {
                memcpy(storage, buffer, to_copy);
            }
            storage[to_copy] = '\0';
            m_len = to_copy;
            sync_view();
        }
        
    private:
        char storage[N+1];
};

class DynamicString : public string {
    public:
        explicit DynamicString(size_t initial_capacity) 
            : string(calc_min_cap(initial_capacity), new char[calc_min_cap(initial_capacity) + 1]) {
            m_owns_memory = true;
            buffer[0] = '\0';
            sync_view();
        }

        DynamicString(const char* cstr) : string(cstr) {}

        DynamicString(const DynamicString& other) 
            : string(calc_min_cap(other.capacity()), new char[calc_min_cap(other.capacity()) + 1]) { 
             m_owns_memory = true;
             string::operator=(other); 
        }
        DynamicString(DynamicString&& other) noexcept : string(static_cast<DynamicString&&>(other)) {}

        using string::operator=; 

        DynamicString& operator=(const DynamicString& other) {
            if (this != &other) {
                if (other.size() > capacity_) resize(other.size());
                string::operator=(other);
            }
            return *this;
        }

        DynamicString& operator=(DynamicString&& other) noexcept {
            string::operator=(static_cast<DynamicString&&>(other)); // Fixed for Arduino
            return *this;
        }

        void resize(size_t min_capacity) {
            if (min_capacity <= capacity_) return;
            size_t new_cap;
            
            if (capacity_ == 0) new_cap = min_capacity;
            else if (capacity_ < 64) new_cap = capacity_ * 2;
            else new_cap = capacity_ + 16; 
            
            if (new_cap < min_capacity) new_cap = min_capacity + 16; 
            if (new_cap < 8) new_cap = 8;

            char* new_buf = new char[new_cap + 1];
            if (!new_buf) return;

            if (m_len > 0) memcpy(new_buf, buffer, m_len);
            new_buf[m_len] = '\0';

            delete[] buffer;
            buffer = new_buf;
            capacity_ = new_cap;
            sync_view();
        }

        bool concat(const char* str) override {
            if (str == nullptr) return false;
            size_t str_len = strlen(str);
            if (m_len + str_len > capacity_) resize(m_len + str_len);
            return string::concat(str);
        }

        bool concat(char c) override {
            if (m_len + 1 > capacity_) resize(m_len + 1);
            return string::concat(c);
        }
        
        bool concat(const string_view& sv) override {
            if (m_len + sv.size() > capacity_) resize(m_len + sv.size());
            return string::concat(sv);
        }

        bool replace(const char* old_str, const char* new_str) override {
            int index = find(old_str);
            if (index == -1) return false;

            size_t old_len = strlen(old_str);
            size_t new_len = strlen(new_str);
            
            size_t projected_len;
            if (new_len >= old_len) projected_len = m_len + (new_len - old_len);
            else projected_len = m_len - (old_len - new_len);

            if (projected_len > capacity_) resize(projected_len);
            return string::replace(old_str, new_str);
        }
};