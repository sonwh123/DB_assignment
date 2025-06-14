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
	int level = 0;
	page* curr = root; // 현재 노드

	while(curr->get_type() == INTERNAL){
		path[level++] = curr; // 경로저장

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

		curr = next;
	}
	// leaf에서 insert 성공 시
	if (curr->insert(key, val)) return;
	// leaf에서 insert 실패 시
	// split 발생
    char* parent_key = nullptr;
    page* new_node = curr->split(key, val, &parent_key);

    while (level > 0) {
        page* parent = path[--level];
        if (parent->insert(parent_key, (uint64_t)new_node)) { return; }
        new_node = parent->split(parent_key, (uint64_t)new_node, &parent_key);
    }

	// root까지 가득 차면 새로운 root 생성
    page* new_root = new page(INTERNAL);
    new_root->set_leftmost_ptr(root);
    new_root->insert(parent_key, (uint64_t)new_node);
    root = new_root;
    height++;

	delete[] parent_key;
}

uint64_t btree::lookup(char *key){
	page* curr = root; // 현재 노드
	return curr->find(key);
}
