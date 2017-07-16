primeify: primeify.c lodepng.c
	gcc -o main $^ -lgmp -lpthread
