CXX := g++
CFLAGS := -std=c++17 -I./ -lrt -lpthread -O3

tests: perf_lin perf_cuc perf_level perf_path perf_cceh

perf_cceh: CCEH
	$(CXX) $(CFLAGS) -o bin/performance_test.exe test/performance_test.cpp src/CCEH.o -DEP
	bin/performance_test.exe

perf_lin: linear_hash
	$(CXX) $(CFLAGS) -o bin/performance_test.exe test/performance_test.cpp src/linear_hash.o -DLIN
	bin/performance_test.exe

perf_cuc: cuckoo_hash
	$(CXX) $(CFLAGS) -o bin/performance_test.exe test/performance_test.cpp src/cuckoo_hash.o -DCUC
	bin/performance_test.exe

input_gen:
	$(CXX) $(CLFAGS) -o bin/input_gen.exe util/input_gen.cpp

linear_hash: src/linear_hash.cpp src/linear_hash.h
	$(CXX) $(CFLAGS) -c -o src/linear_hash.o src/linear_hash.cpp

CCEH: src/CCEH.cpp src/CCEH.h
	$(CXX) $(CFLAGS) -c -o src/CCEH.o src/CCEH.cpp

cuckoo_hash: src/cuckoo_hash.cpp src/cuckoo_hash.h
	$(CXX) $(CFLAGS) -c -o src/cuckoo_hash.o src/cuckoo_hash.cpp

new_level: external/new_level_hashing.c external/new_level_hashing.h external/log.h external/log.c
	$(CXX) $(CFLAGS) -c -o external/log.o external/log.c
	$(CXX) $(CFLAGS) -c -o external/new_level_hashing.o external/new_level_hashing.c

level: external/level_hashing.c external/level_hashing.h
	$(CXX) $(CFLAGS) -c -o external/ori_level_hashing.o external/level_hashing.c

path: external/path_hashing.c external/path_hashing.h
	$(CXX) $(CFLAGS) -c -o external/path_hashing.o external/path_hashing.c

bch_old: external/bucketized_cuckoo.c external/bucketized_cuckoo.h
	$(CXX) $(CFLAGS) -c -o external/bucketized_cuckoo.o external/bucketized_cuckoo.c

bch_perf: bch_old
	$(CXX) $(CFLAGS) -o bin/bch_test.exe test/external_test.cpp external/bucketized_cuckoo.o -DBCH
	bin/bch_test.exe

level_new: external/level_hashing.cpp external/level_hashing.hpp
	$(CXX) $(CFLAGS) -c -o external/level_hashing.o external/level_hashing.cpp

perf_level: level_new
	$(CXX) $(CFLAGS) -o bin/performance_test.exe test/performance_test.cpp external/level_hashing.o -DLEVEL
	bin/performance_test.exe

perf_nlevel: new_level
	$(CXX) $(CFLAGS) -o bin/performance_test.exe test/external_test.cpp external/new_level_hashing.o external/log.o -DNLEVEL
	bin/performance_test.exe

path_new: external/path_hashing.cpp external/path_hashing.hpp
	$(CXX) $(CFLAGS) -c -o external/path_hashing.o external/path_hashing.cpp

perf_path: path_new
	$(CXX) $(CFLAGS) -o bin/performance_test.exe test/performance_test.cpp external/path_hashing.o -DPATH
	bin/performance_test.exe

bch: external/bucketized_cuckoo.cpp external/bucketized_cuckoo.hpp
	$(CXX) $(CFLAGS) -c -o external/bucketized_cuckoo.o external/bucketized_cuckoo.cpp

perf_bch: bch
	$(CXX) $(CFLAGS) -o bin/performance_test.exe test/performance_test.cpp external/bucketized_cuckoo.o -DBCH
	bin/performance_test.exe

search_with_failure: CCEH #path_new level_new cuckoo_hash linear_hash CCEH bch
	echo EXTENDIBLE
	$(CXX) $(CFLAGS) -o bin/search_with_failure.exe test/search_with_failure.cpp src/CCEH.o -DEP
	bin/search_with_failure.exe
	# echo LINEAR
	# $(CXX) $(CFLAGS) -o bin/search_with_failure.exe test/search_with_failure.cpp src/linear_hash.o -DLIN
	# bin/search_with_failure.exe
	# echo CUCKOO
	# $(CXX) $(CFLAGS) -o bin/search_with_failure.exe test/search_with_failure.cpp src/cuckoo_hash.o -DCUC
	# bin/search_with_failure.exe
	# echo PATH
	# $(CXX) $(CFLAGS) -o bin/search_with_failure.exe test/search_with_failure.cpp external/path_hashing.o -DPATH
	# bin/search_with_failure.exe
	# echo LEVEL
	# $(CXX) $(CFLAGS) -o bin/search_with_failure.exe test/search_with_failure.cpp external/level_hashing.o -DLEVEL
	# bin/search_with_failure.exe
	# echo BCH
	# $(CXX) $(CFLAGS) -o bin/search_with_failure.exe test/search_with_failure.cpp external/bucketized_cuckoo.o -DBCH
	# bin/search_with_failure.exe

block_size: CCEH
	$(CXX) $(CFLAGS) $(PERF_FLAG) -o bin/block_size.exe test/block_size.cpp src/CCEH.o -DEP # libnvmemul.so
	bin/block_size.exe

performance_breakdown: path_new level_new cuckoo_hash linear_hash CCEH bch level
	echo EXTENDIBLE
	$(CXX) $(CFLAGS) -o bin/performance_breakdown.exe test/performance_breakdown.cpp src/CCEH.o -DEP libnvmemul.so
	cp bin/performance_breakdown.exe ~/quartz/scripts/bin/extendible.x
	# bin/performance_breakdown.exe
	echo LINEAR
	$(CXX) $(CFLAGS) -o bin/performance_breakdown.exe test/performance_breakdown.cpp src/linear_hash.o -DLIN libnvmemul.so
	cp bin/performance_breakdown.exe ~/quartz/scripts/bin/linear.x
	# bin/performance_breakdown.exe
	echo CUCKOO
	$(CXX) $(CFLAGS) -o bin/performance_breakdown.exe test/performance_breakdown.cpp src/cuckoo_hash.o -DCUC libnvmemul.so
	cp bin/performance_breakdown.exe ~/quartz/scripts/bin/cuckoo.x
	# bin/performance_breakdown.exe
	echo PATH
	$(CXX) $(CFLAGS) -o bin/performance_breakdown.exe test/performance_breakdown.cpp external/path_hashing.o -DPATH libnvmemul.so
	cp bin/performance_breakdown.exe ~/quartz/scripts/bin/path.x
	# bin/performance_breakdown.exe
	echo LEVEL
	$(CXX) $(CFLAGS) -o bin/performance_breakdown.exe test/performance_breakdown.cpp external/level_hashing.o -DLEVEL libnvmemul.so
	cp bin/performance_breakdown.exe ~/quartz/scripts/bin/level.x
	# bin/performance_breakdown.exe
	echo BCH
	$(CXX) $(CFLAGS) -o bin/performance_breakdown.exe test/performance_breakdown.cpp external/bucketized_cuckoo.o -DBCH libnvmemul.so
	cp bin/performance_breakdown.exe ~/quartz/scripts/bin/bch.x
	$(CXX) $(CFLAGS) -o bin/performance_breakdown.exe test/level_ori.cpp external/ori_level_hashing.o libnvmemul.so
	cp bin/performance_breakdown.exe ~/quartz/scripts/bin/level_ori.x
	# bin/performance_breakdown.exe

perf_tool: bch #path_new level_new cuckoo_hash linear_hash CCEH bch
	# echo EXTENDIBLE
	# $(CXX) $(CFLAGS) $(PERF_FLAG) -o bin/perf_tool.exe test/perf_tool.cpp src/CCEH.o -DEP
	# bin/perf_tool.exe
	# echo LINEAR
	# $(CXX) $(CFLAGS) $(PERF_FLAG) -o bin/perf_tool.exe test/perf_tool.cpp src/linear_hash.o -DLIN
	# bin/perf_tool.exe
	# echo CUCKOO
	# $(CXX) $(CFLAGS) $(PERF_FLAG) -o bin/perf_tool.exe test/perf_tool.cpp src/cuckoo_hash.o -DCUC
	# bin/perf_tool.exe
	# echo PATH
	# $(CXX) $(CFLAGS) $(PERF_FLAG) -o bin/perf_tool.exe test/perf_tool.cpp external/path_hashing.o -DPATH
	# bin/perf_tool.exe
	# echo LEVEL
	# $(CXX) $(CFLAGS) $(PERF_FLAG) -o bin/perf_tool.exe test/perf_tool.cpp external/level_hashing.o -DLEVEL
	# bin/perf_tool.exe
	echo BCH
	$(CXX) $(CFLAGS) $(PERF_FLAG) -o bin/perf_tool.exe test/perf_tool.cpp external/bucketized_cuckoo.o -DBCH
	bin/perf_tool.exe

datasize: CCEH # path_new level_new cuckoo_hash linear_hash CCEH bch
	echo EXTENDIBLE
	$(CXX) $(CFLAGS) $(PERF_FLAG) -o bin/datasize.exe test/datasize.cpp src/CCEH.o -DEP
	bin/datasize.exe
	# echo LINEAR
	# $(CXX) $(CFLAGS) -o bin/datasize.exe test/datasize.cpp src/linear_hash.o -DLIN
	# bin/datasize.exe
	# echo CUCKOO
	# $(CXX) $(CFLAGS) -o bin/datasize.exe test/datasize.cpp src/cuckoo_hash.o -DCUC
	# bin/datasize.exe
	# echo PATH
	# $(CXX) $(CFLAGS) -o bin/datasize.exe test/datasize.cpp external/path_hashing.o -DPATH
	# bin/datasize.exe
	# echo LEVEL
	# $(CXX) $(CFLAGS) -o bin/datasize.exe test/datasize.cpp external/level_hashing.o -DLEVEL
	# bin/datasize.exe
	# echo BCH
	# $(CXX) $(CFLAGS) -o bin/datasize.exe test/datasize.cpp external/bucketized_cuckoo.o -DBCH
	# bin/datasize.exe

all:
	echo "NOTHING YET"


clean:
	rm -rf src/*.o bin/* util/*.o external/*.o
