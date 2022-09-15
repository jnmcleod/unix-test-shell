CXX = g++
OBJ = main.o shell.o
FLAGS = -std=gnu++0x
EXEC = myshell

$(EXEC): $(OBJ)
	$(CXX) $(FLAGS) -o $(EXEC) $(OBJ)

main.o: main.cpp shell.hpp
	g++ $(FLAGS) -c main.cpp

shell.o: shell.cpp shell.hpp
	g++ $(FLAGS) -c shell.cpp

run: $(EXEC)
	./$(EXEC)

debug: $(EXEC)
	lldb $(EXEC)

memcheck: $(EXEC)
	valgrind --leak-check=yes ./$(EXEC)

clean:
	rm -f $(EXEC) $(OBJ)
	rm -f core* vgcore*

