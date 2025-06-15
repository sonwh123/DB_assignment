#include "btree.hpp"
#include <iostream> 

btree::btree(){
	root = new page(LEAF);
	height = 1;
};

// page.cpp에만 정의되어 있으므로 선언
uint16_t get2byte(void *dest);

void btree::insert(char *key, uint64_t val){
	// root에서 leaf까지 내려가기
	page* path[10];
	uint64_t versions[10];
	int level = 0;
	page* curr = root; // 현재 노드

	while(curr->get_type() == INTERNAL){
		path[level] = curr; // 경로저장
		versions[level] = curr->read_version(); // 버전저장
        if (!curr->try_read_lock(versions[level])) return insert(key, val); // read lock 시도

		page* next = nullptr;

		// 다음 노드 찾기
		uint32_t num_data = *(uint32_t*)((uint8_t*)curr+4); // page 기준 4byte
		void* offset_array = *(void**)((uint8_t*)curr +8); // page 기준 8byte
		void* pre_record = 0; // 이전 레코드
		for (int i = 0; i < num_data; i++) {
			uint16_t offset = get2byte((uint8_t*)offset_array + i * 2);
			void* record = (uint8_t*)curr + offset;
			char* stored_key = curr->get_key(record);

			// 찾고자 하는 키보다 큰 키를 만나면 child로 내려감
			if (strcmp(key, stored_key) < 0) {
				if (pre_record == 0){
					next = curr->get_leftmost_ptr(); // 이전 key가 없으면 page의 가장 왼쪽 주소
				}else {
					next = (page*)curr->get_val(pre_record); // 이전 key가 있으면 해당 value값으로 child 주소 찾기
				}
				break;
			}
			pre_record = record;
		}

		if (next == nullptr) { 
			// 가장 오른쪽 자식 포인터 찾기
			num_data = *(uint32_t*)((uint8_t*)curr+4); // page 기준 4byte
			offset_array = *(void**)((uint8_t*)curr +8); // page 기준 8byte
			uint16_t offset = get2byte((uint8_t*)offset_array + (num_data-1) * 2);
			void* record = (uint8_t*)curr + offset;
			char* k = curr->get_key(record);
			next = (page*)(curr->get_val(k)); // 가장 오른쪽 자식 포인터
		}

		uint64_t next_ver = next->read_version();
        if (!next->try_read_lock(next_ver)) return insert(key, val); // read lock 시도
        if (!curr->read_unlock(versions[level])) return insert(key, val); // read lock 풀기

        curr = next;
	}
	// leaf에서 insert 성공 시
	if (!curr->try_write_lock()) return insert(key, val); // write lock 시도

    if (curr->insert(key, val)) {
        curr->write_unlock(); // write lock 풀기
        return;
    }
	// leaf에서 insert 실패 시
	// split 발생
    char* parent_key = nullptr;
    page* new_node = curr->split(key, val, &parent_key);
	curr->write_unlock(); // write lock 풀기

    while (level > 0) {
        page* parent = path[--level];
		if (!parent->try_write_lock()) return insert(key, val); // write lock 시도

        if (parent->insert(parent_key, (uint64_t)new_node)) {
            parent->write_unlock(); // write lock 풀기
            return;
        }
        new_node = parent->split(parent_key, (uint64_t)new_node, &parent_key);
		parent->write_unlock(); // write lock 풀기
    }

	// root까지 가득 차면 새로운 root 생성
    page* new_root = new page(INTERNAL);
	if (!new_root->try_write_lock()) return insert(key, val); // write lock 시도

    new_root->set_leftmost_ptr(root);
    new_root->insert(parent_key, (uint64_t)new_node);
	new_root->write_unlock(); // write lock 풀기

    root = new_root;
    height++;

	delete[] parent_key;
}

uint64_t btree::lookup(char *key){
	page* curr = root; // 현재 노드
	uint64_t curr_version = curr->read_version(); // 버전저장
	if (!curr->try_read_lock(curr_version)) return lookup(key); // read lock 시도
	return curr->find(key);
}