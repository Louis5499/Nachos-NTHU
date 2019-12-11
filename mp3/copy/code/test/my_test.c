#include<syscall.h>


int main(void) {

	char filename[] = "apple";
	int fd = Open(filename);

        MSG("file Descriptor, " << fd);

	Halt();
}
