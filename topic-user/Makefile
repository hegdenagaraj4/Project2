CXX := clang 
CXXFLAGS := -g -w -fPIC -Wall
INCLUDES := -I

OBJECTS	:= useraction.o userfunctions.o

useraction	: $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o myipc

userfunctions	: userfunctions.o
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o userfunctions

useraction.o	: useraction.c
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c useraction.c -o useraction.o

userfunctions.o	: userfunctions.c
	$(CXX) $(CXXFLAGS) -c userfunctions.c -o userfunctions.o

clean	:
	rm -rf myipc *.o
