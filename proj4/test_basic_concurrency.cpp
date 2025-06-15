#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cassert>
#include "btree.hpp"

#define STRING_LEN 20
#define NUM_KEYS 8
#define NUM_THREADS 2

btree tree;
std::vector<std::string> keys;

// STEP 1: 인덱스 빌드 (싱글 쓰레드 insert)
void index_build() {
    for (int i = 0; i < NUM_KEYS; ++i) {
        std::string key(STRING_LEN - 1, 'a' + (i % 26));
        key[0] = 'a' + (i % 26);
        keys.push_back(key);
        tree.insert(const_cast<char*>(key.c_str()), i * 10);
    }
    std::cout << "[STEP 1] Index build complete (" << NUM_KEYS << " keys inserted)\n";
}

// STEP 2-1: 동시 lookup 수행
void lookup_worker() {
    int error_cnt = 0;
    for (int i = 0; i < NUM_KEYS; ++i) {
        std::string key = keys[i];
        uint64_t expected = i * 10;
        uint64_t result = tree.lookup(const_cast<char*>(key.c_str()));
        if (result != expected) {
            std::cout << "❌ Key: " << key << ", Expected: " << expected << ", Got: " << result << std::endl;
            error_cnt++;
        }
    }
    std::cout << "[Lookup Thread] 완료 (오류 " << error_cnt << "개)\n";
}

// STEP 2-2: 추가 insert 수행
void insert_worker() {
    for (int i = 0; i < 10; ++i) {
        std::string key = "newkey_" + std::to_string(i);
        tree.insert(const_cast<char*>(key.c_str()), 9999 + i);
    }
    std::cout << "[Insert Thread] 10개 삽입 완료\n";
}

int main() {
    // Step 1: Index build (단일 쓰레드)
    index_build();

    // Step 2: 다중 쓰레드 읽기/쓰기
    std::thread t1(lookup_worker);
    std::thread t2(insert_worker);

    t1.join();
    t2.join();

    std::cout << "[STEP 2] Concurrent read/write 테스트 완료\n";
    return 0;
}
