CXX = g++
CXXFLAGS = -std=c++17 -O2

all: main1/main1 dyn_lib1/libcompute.so dyn_lib2/libcompute.so main2/main2

# -------------------------
# Static library
# -------------------------

static_lib/numgen.o: static_lib/numgen.cpp static_lib/numgen.h
	$(CXX) $(CXXFLAGS) -Istatic_lib -c static_lib/numgen.cpp -o static_lib/numgen.o

static_lib/libnumgen.a: static_lib/numgen.o
	ar rcs static_lib/libnumgen.a static_lib/numgen.o

# -------------------------
# Main1
# -------------------------

main1/main1: main1/main.cpp static_lib/libnumgen.a
	$(CXX) $(CXXFLAGS) -Imain1 -Istatic_lib main1/main.cpp -Lstatic_lib -lnumgen -o main1/main1

# -------------------------
# Dynamic library 1
# -------------------------

dyn_lib1/compute.o: dyn_lib1/compute.cpp dyn_lib1/compute.h
	$(CXX) $(CXXFLAGS) -fPIC -Idyn_lib1 -c dyn_lib1/compute.cpp -o dyn_lib1/compute.o

dyn_lib1/libcompute.so: dyn_lib1/compute.o
	$(CXX) -shared -o dyn_lib1/libcompute.so dyn_lib1/compute.o

# -------------------------
# Dynamic library 2
# -------------------------

dyn_lib2/compute.o: dyn_lib2/compute.cpp dyn_lib2/compute.h
	$(CXX) $(CXXFLAGS) -fPIC -Idyn_lib2 -c dyn_lib2/compute.cpp -o dyn_lib2/compute.o

dyn_lib2/libcompute.so: dyn_lib2/compute.o
	$(CXX) -shared -o dyn_lib2/libcompute.so dyn_lib2/compute.o

# -------------------------
# Main2
# -------------------------

main2/main2: main2/main.cpp dyn_lib1/libcompute.so
	$(CXX) $(CXXFLAGS) -Imain2 -Idyn_lib1 main2/main.cpp -Ldyn_lib1 -lcompute -o main2/main2

# -------------------------
# Clean
# -------------------------

clean:
	rm -f static_lib/*.o
	rm -f static_lib/*.a
	rm -f dyn_lib1/*.o dyn_lib1/*.so
	rm -f dyn_lib2/*.o dyn_lib2/*.so
	rm -f main1/main1
	rm -f main2/main2