SRCS := main.cpp HttpConn.cpp  ./buffer/Buffer.cpp ./log/Log.cpp TimerQueue.cpp ./buffer/Buffer.h ./log/Log.h
OUT := main

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2
endif

main: $(SRCS)
	$(CXX) -o $(OUT) $^ $(CXXFLAGS) -pthread

clean:
	rm -r $(OUT)