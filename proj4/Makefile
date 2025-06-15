page: slot_header.o page.o main_slotted_page.o
	g++ -c slot_header.cpp
	g++ -c page.cpp
	g++ -c main_slotted_page.cpp
	g++ -o ./page slot_header.o page.o main_slotted_page.o

btree: slot_header.o page.o btree.o main_btree.o
	g++ -c slot_header.cpp
	g++ -c page.cpp
	g++ -c main_btree.cpp
	g++ -c btree.cpp
	g++ -o ./btree slot_header.o page.o btree.o main_btree.o

test_concurrency: 
	g++ -std=c++11 -pthread test_basic_concurrency.cpp btree.cpp page.cpp slot_header.cpp -o test_bptree

clean:
	rm -f page
	rm -f btree
	rm -f test_bptree
	rm -f *.o
