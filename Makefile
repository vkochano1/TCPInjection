ALL:
	g++ -std=c++17  -O3 -I./ -g -I./libs/libtins/include/  -fpermissive -o SessionTest \
	SessionTest.cpp  QPSocketCfg.cpp QPSocket.cpp InjectionsAndRejections.cpp \
	-lstdc++ -libverbs ./libs/libtins/lib/libtins.so
	g++   -std=c++11 Trash.cpp -o ./Trash -pthread
