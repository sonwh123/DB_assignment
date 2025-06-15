#include "page.hpp"
#include <iostream> 
#include <cstring> 

void put2byte(void *dest, uint16_t data){
	*(uint16_t*)dest = data;
}

uint16_t get2byte(void *dest){
	return *(uint16_t*)dest;
}

page::page(uint16_t type){
	memset(this, 0, sizeof(page)); // 메모리 초기화

	hdr.set_num_data(0);
	hdr.set_data_region_off(PAGE_SIZE-1-sizeof(page*));
	hdr.set_offset_array((void*)((uint64_t)this+sizeof(slot_header)));
	hdr.set_page_type(type);

	version = 0;
}

uint16_t page::get_type(){
	return hdr.get_page_type();
}

uint16_t page::get_record_size(void *record){
	uint16_t size = *(uint16_t *)record;
	return size;
}

char *page::get_key(void *record){
	char *key = (char *)((uint64_t)record+sizeof(uint16_t));
	return key;
}

uint64_t page::get_val(void *key){
	uint64_t val= *(uint64_t*)((uint64_t)key+(uint64_t)strlen((char*)key)+1);
	return val;
}

void page::set_leftmost_ptr(page *p){
	leftmost_ptr = p;
}

page *page::get_leftmost_ptr(){
	return leftmost_ptr;	
}

uint64_t page::find(char *key){
	// INTERNAL인 경우: 자식 노드로 이동
	if (get_type() == INTERNAL) {
		void* offset_array = hdr.get_offset_array();
		uint32_t num_data = hdr.get_num_data();
		uint16_t pre_offset = NULL; // 이전 레코드

		for (int i = 0; i < num_data; i++) {
			uint16_t offset = get2byte((uint8_t*)offset_array + i * 2);
			void* record = (uint8_t*)this + offset;
			char* stored_key = get_key(record);
			// key < stored_key 면 왼쪽 자식으로 이동
			if (strcmp(key, stored_key) < 0) {
				if (!pre_offset){
					return get_leftmost_ptr()->find(key); // 이전 key가 없으면 page의 가장 왼쪽 주소
				}
				else{
					void* record = (uint8_t*)this + pre_offset;
					return ((page*)get_val(get_key(record)))->find(key); // 이전 key가 있으면 해당 value값으로 child 주소 찾기
				}
			}
			
			pre_offset = offset;
		}
		// 모든 키보다 크면, 마지막 엔트리 오른쪽 포인터로
		uint16_t offset = get2byte((uint8_t*)offset_array + (num_data - 1) * 2);
		void* record = (uint8_t*)this + offset;
		page* child = (page*)get_val(get_key(record));
		return child->find(key);
	}
	
	// leaf일 경우 (기존 코드)
	uint32_t num_data = hdr.get_num_data();
	void* offset_array = hdr.get_offset_array();
	// key 값 비교
	for (int i = 0; i < num_data; i++){
		uint16_t offset = get2byte((uint8_t*)offset_array + i * 2);
		void* temp_addr = (uint8_t*)this + offset;
		char* temp_key = get_key(temp_addr);

		if (strcmp(temp_key, key) == 0) {// 같으면 해당 key값의 value 출력
			return get_val((void*)temp_key);
		}
	}

	// 없으면 0 출력
	return 0;
}

bool page::insert(char *key,uint64_t val){
	uint16_t key_len = strlen(key) + 1; // null 포함
	uint16_t record_size = sizeof(uint16_t) + key_len + sizeof(uint64_t);
	// 여유공간 확인
	/*
	if(is_full(record_size)){
		return false;
	}
	*/
	if(hdr.get_num_data() == 7){ // is_full 함수 오류
		return false;
	}
	
	// record 저장
	uint16_t data_off = hdr.get_data_region_off();
	data_off = data_off - record_size + 1;
	void* record_ptr = (uint8_t*)this + data_off; // record 저장 시작 위치
	
	put2byte(record_ptr, record_size); // record size 저장
	memcpy((uint8_t*)record_ptr + 2, key, key_len); // key 저장
	memcpy((uint8_t*)record_ptr + 2 + key_len, &val, sizeof(uint64_t)); // value 저장
	
	// offset 업데이트
	void* offset_array = hdr.get_offset_array();
	uint32_t num_data = hdr.get_num_data();
	int insert_index = 0;

	// 들어갈 위치 찾기
	while(insert_index < num_data){
		uint16_t offset = get2byte((uint8_t*)offset_array + insert_index * 2);
		void* temp_record = (uint8_t*)this + offset;
		char* temp_char = get_key(temp_record);

		if(strcmp(key, temp_char)<0) // key 값보다 크면 멈춤
			break;
		insert_index++;
	}

	// offset array 뒤로 밀기
	for(int i = num_data; i > insert_index; i--){
		uint16_t prev_off = get2byte((uint8_t*)offset_array + (i-1) * 2);
		put2byte((uint8_t*)offset_array + i * 2, prev_off);
	}

	// offset 삽입
	put2byte((uint8_t*)offset_array + insert_index * 2, data_off);

	// header 업데이트
	hdr.set_data_region_off(data_off - 1);
	hdr.set_num_data(num_data + 1);

	return true;
}

page* page::split(char *key, uint64_t val, char** parent_key) {
	page* new_page = new page(get_type());

	// offset array 접근
	void* offset_array = hdr.get_offset_array();
	// 레코드 수 세기
	int num_data = hdr.get_num_data();

	// split 기준점
	int mid = num_data / 2;

	// new_page에 절반 저장
	for (int i = mid; i < num_data; i++){
		uint16_t off = *(uint16_t *)((uint64_t)offset_array + i * 2);
		void *data_region = (void *)((uint64_t)this + (uint64_t)off);
		char* restored_key = get_key(data_region);
		new_page->insert(restored_key, get_val((void*)restored_key));
	}
	new_page->insert(key, val); // 마지막(최근) 데이터

	// parent_key 추출
	uint16_t off = *(uint16_t *)((uint64_t)offset_array + mid * 2);
	void *data_region = (void *)((uint64_t)this + (uint64_t)off);
	char* src_key = get_key(data_region);
	*parent_key = new char[strlen(src_key) + 1];
	strcpy(*parent_key, src_key);

	// this 초기화
	defrag();

	return new_page;
}


bool page::is_full(uint64_t inserted_record_size){
	// offset_array 마지막 위치 = offset_array 시작 위치 + 데이터 개수 * 2
	uint16_t offset_array_end = (uint64_t)hdr.get_offset_array() + (hdr.get_num_data()-1)*2;
	// free_space 크기 = record 영역의 가장 왼쪽 - offset_array_end + 1
	uint16_t free_space = hdr.get_data_region_off() - offset_array_end + 1;
	if(free_space >= inserted_record_size + 2)
		return false;
	else
		return true;
}

void page::defrag(){
	page *new_page = new page(get_type());
	int num_data = hdr.get_num_data();
	void *offset_array=hdr.get_offset_array();
	void *stored_key=nullptr;
	uint16_t off=0;
	uint64_t stored_val=0;
	void *data_region=nullptr;

	for(int i=0; i<num_data/2; i++){
		off= *(uint16_t *)((uint64_t)offset_array+i*2);	
		data_region = (void *)((uint64_t)this+(uint64_t)off);
		stored_key = get_key(data_region);
		stored_val= get_val((void *)stored_key);
		new_page->insert((char*)stored_key,stored_val);
	}	
	new_page->set_leftmost_ptr(get_leftmost_ptr());

	memcpy(this, new_page, sizeof(page));
	hdr.set_offset_array((void*)((uint64_t)this+sizeof(slot_header)));
	delete new_page;

}

void page::print(){
	uint32_t num_data = hdr.get_num_data();
	uint16_t off=0;
	uint16_t record_size= 0;
	void *offset_array=hdr.get_offset_array();
	void *stored_key=nullptr;
	uint64_t stored_val=0;

	printf("## slot header\n");
	printf("Number of data :%d\n",num_data);
	printf("offset_array : |");
	for(int i=0; i<num_data; i++){
		off= *(uint16_t *)((uint64_t)offset_array+i*2);	
		printf(" %d |",off);
	}
	printf("\n");

	void *data_region=nullptr;
	for(int i=0; i<num_data; i++){
		off= *(uint16_t *)((uint64_t)offset_array+i*2);	
		data_region = (void *)((uint64_t)this+(uint64_t)off);
		record_size = get_record_size(data_region);
		stored_key = get_key(data_region);
		stored_val= get_val((void *)stored_key);
		printf("==========================================================\n");
		printf("| data_sz:%u | key: %s | val :%lu | key_len:%lu\n",record_size,(char*)stored_key, stored_val,strlen((char*)stored_key));

	}
}

// 현재 버전 반환
uint64_t page::read_version(){
	return version.load();
}

// 버전을 통해 페이지가 바뀌었는지 확인
bool page::validate_read(uint64_t old_version){
	return read_version() == old_version;
}

// 페이지 버전이 읽기가 가능한지 확인
bool page::try_read_lock(uint64_t curr_version){
	return curr_version % 2 == 0;
}

// 페이지를 사용 가능한지 확인한 후 atomic 연산으로 쓰기 락을 획득
bool page::try_write_lock(){
	uint64_t expected = read_version();
    if (expected % 2 != 0) return false; // 이미 lock된 상태

    // expected가 짝수일 경우, expected+1로 atomic하게 바꾸기
    return version.compare_exchange_strong(expected, expected + 1);
}

// 버전을 통해 읽기 작업 중 페이지가 변경되었는지 확인
bool page::read_unlock(uint64_t old_version){
	return validate_read(old_version);
}

// 쓰기 작업 이후 버전을 갱신
void page::write_unlock(){
	version.fetch_add(1);
}


